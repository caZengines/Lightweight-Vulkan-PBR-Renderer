#include "render/renderer.hpp"

#include <array>
#include <cassert>
#include <cstring>
#include <stdexcept>

#define VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "camera.hpp"
#include "command_manager.hpp"
#include "platform/window.hpp"
#include "rhi/rhi_factory.hpp"
#include "render/command_recorder.hpp"
#include "render/frame_resources.hpp"
#include "render/frame_uniforms.hpp"
#include "render/pipeline.hpp"
#include "render/pipeline_cache.hpp"
#include "render/pipeline_spec.hpp"
#include "render/shader_manager.hpp"
#include "render/swapchain.hpp"
#include "render_context.hpp"

namespace render {

Renderer::Renderer(Dependencies deps, const RenderSettings& settings)
    : window_(deps.window),
      surface_(deps.surface),
      rct_(deps.rct),
      camera_(deps.camera),
      graphicsPool_(deps.graphicsPool),
      rhiFactory_(deps.factory),
      settings_(settings),
      spirvPath_(deps.spirvPath),
      setLayouts_(std::move(deps.setLayouts)) {
    swapchain_ = std::make_unique<Swapchain>(rct_, deps.alloc, surface_, window_,
                                             rhiFactory_, settings_);
    frames_ = std::make_unique<FrameResources>();
    frames_->init(rct_,
                  graphicsPool_,
                  deps.alloc,
                  deps.set0Pool,
                  setLayouts_[0],
                  static_cast<std::uint32_t>(swapchain_->Image_.images.size()));
    createPipeline();
}

Renderer::~Renderer() {
    if (!cleaned_) cleanup();
}

void Renderer::createPipeline() {
    // Depth format: taken from the Swapchain so attachments and pipeline agree
    // by construction (removes the old Pipeline::findDepthFormat duplicate).
    GraphicsPipelineSpec spec;
    spec.colorFormat = swapchain_->getSurfaceFormat().format;
    spec.depthFormat = swapchain_->depthFormat();
    spec.msaaSamples = settings_.msaaSamples;

    shaders_      = std::make_unique<ShaderManager>();
    pipelineCache_ = std::make_unique<PipelineCache>();
    const auto& pipeline =
        pipelineCache_->getOrCreate(rct_, setLayouts_, spec, *shaders_, spirvPath_);
    recorder_ = std::make_unique<CommandRecorder>(*swapchain_, pipeline, rhiFactory_);
}

void Renderer::cleanup() {
    rct_.device.waitIdle();
    swapchain_->cleanupSwapChain();
    cleaned_ = true;
    // FrameResources members release their own Vulkan handles via RAII after
    // the idle wait above (same net ordering as the legacy explicit teardown).
}

std::optional<Renderer::FrameContext> Renderer::beginFrame() {
    auto fenceResult =
        rct_.device.waitForFences(*frames_->inFlightFence(frameCursor_), vk::True, UINT64_MAX);
    if (fenceResult != vk::Result::eSuccess) {
        throw std::runtime_error("failed to wait for fence!");
    }

    auto [result, imageIndex] =
        swapchain_->swapChain().acquireNextImage(UINT64_MAX,
                                                 *frames_->presentComplete(frameCursor_),
                                                 nullptr);
    if (result == vk::Result::eErrorOutOfDateKHR) {
        recreateAfterResize();
        return std::nullopt;  // skip this frame, like legacy early-return path
    }
    if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
        throw std::runtime_error("failed to acquire swap chain image!");
    }

    fillUniformBuffer(frameCursor_);
    writeFrameSet(frameCursor_);

    // Only reset the fence when we are actually submitting work.
    rct_.device.resetFences(*frames_->inFlightFence(frameCursor_));
    frames_->commandBuffer(frameCursor_).reset();

    return FrameContext{imageIndex, frameCursor_};
}

void Renderer::record(FrameContext& ctx, std::span<const RenderItem> items) {
    recorder_->record(frames_->commandBuffer(ctx.frameIndex),
                      ctx.imageIndex,
                      frames_->frameSetHandles()[ctx.frameIndex],
                      items);
}

void Renderer::endFrame(const FrameContext& ctx) {
    const std::uint64_t signalValue = frames_->nextSignalValue();
    std::array<vk::Semaphore, 2> signalSemaphores{
        *frames_->renderTimeline(),
        *frames_->presentWait(ctx.imageIndex)};
    std::array<std::uint64_t, 2> signalValues{signalValue, 0};

    vk::TimelineSemaphoreSubmitInfo timelineSubmitInfo;
    timelineSubmitInfo.setSignalSemaphoreValueCount(2)
                      .setPSignalSemaphoreValues(signalValues.data());

    vk::PipelineStageFlags waitDestinationStageMask(
        vk::PipelineStageFlagBits::eColorAttachmentOutput);
    const vk::SubmitInfo submitInfo = [&] {
        vk::SubmitInfo info;
        info.setWaitSemaphores(*frames_->presentComplete(ctx.frameIndex))
            .setWaitDstStageMask(waitDestinationStageMask)
            .setCommandBuffers(*frames_->commandBuffer(ctx.frameIndex))
            .setSignalSemaphores(signalSemaphores)
            .setPNext(&timelineSubmitInfo);
        return info;
    }();
    graphicsPool_.queue().submit(submitInfo, *frames_->inFlightFence(ctx.frameIndex));

    const vk::PresentInfoKHR presentInfoKHR = [&] {
        vk::PresentInfoKHR info;
        info.setWaitSemaphores(*frames_->presentWait(ctx.imageIndex))
            .setSwapchains(*swapchain_->swapChain())
            .setImageIndices(ctx.imageIndex);
        return info;
    }();
    const auto result = graphicsPool_.queue().presentKHR(presentInfoKHR);
    if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR ||
        framebufferResized) {
        framebufferResized = false;
        recreateAfterResize();
    } else {
        assert(result == vk::Result::eSuccess);
    }

    frameCursor_ = (frameCursor_ + 1) % kMaxFramesInFlight;
}

void Renderer::recreateAfterResize() {
    rct_.device.waitIdle();
    frameCursor_ = 0;
    swapchain_->recreateSwapChain(surface_, window_);
    frames_->recreateSync(static_cast<std::uint32_t>(swapchain_->Image_.images.size()));
}

void Renderer::fillUniformBuffer(std::uint32_t frame) {
    UniformBufferObject ubo{};
    ubo.view = camera_.viewMatrix();
    ubo.proj = glm::perspective(
        glm::radians(45.0f),
        static_cast<float>(swapchain_->getExtent().width) /
            static_cast<float>(swapchain_->getExtent().height),
        0.1f, 100.0f);
    ubo.proj[1][1] *= -1;

    ubo.camPos        = glm::vec4(camera_.position(), 1.0f);
    ubo.light.pos     = glm::vec4(4.0f, 20.0f, -25.0f, 1.0f);   // light params move to content in Phase 5
    ubo.light.color   = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    ubo.light.intensity = 10.0f;

    std::memcpy(frames_->uniformBuffer(frame).mappedData(), &ubo, sizeof(ubo));
}

void Renderer::writeFrameSet(std::uint32_t frame) {
    vk::DescriptorBufferInfo bufferInfo{};
    bufferInfo.setBuffer(frames_->uniformBuffer(frame).getHandle())
              .setOffset(0)
              .setRange(sizeof(UniformBufferObject));

    vk::WriteDescriptorSet write{};
    write.setDstSet(frames_->frameSetHandles()[frame])
         .setDstBinding(0)
         .setDescriptorType(vk::DescriptorType::eUniformBuffer)
         .setBufferInfo(bufferInfo);
    rct_.device.updateDescriptorSets(write, {});
}

}  // namespace render
