#include "render/renderer.hpp"
#include "platform/log.hpp"
#include "vulkan/vulkan.hpp"


#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

#define VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "rhi/command_pool.hpp"
#include "rhi/swapchain.hpp"
#include "render/command_recorder.hpp"
#include "render/descriptor_set_layout_library.hpp"
#include "render/frame_resources.hpp"
#include "render/frame_uniforms.hpp"
#include "render/pipeline.hpp"
#include "render/pipeline_layout_library.hpp"
#include "render/pipeline_spec.hpp"
#include "resource/shader_library.hpp"
#include "render_context.hpp"
#include "scene/camera_manager.hpp"

namespace render {

namespace {

// rhi reports the raw surface capabilities; the controller only cares about the
// present-timing subset.
auto toSurfaceSupport(const rhi::SurfaceTimingCapabilities& caps) {
    SurfaceTimingSupport support{};
    support.capabilitiesKnown   = caps.queried;
    support.presentTiming       = caps.presentTimingSupported;
    support.presentAtAbsolute   = caps.presentAtAbsoluteTimeSupport;
    support.presentAtRelative   = caps.presentAtRelativeTimeSupport;
    support.presentStageQueries = caps.presentStageQueries;
    return support;
}

}  // namespace

void Renderer::PresentTimingController::setSurfaceSupport(SurfaceTimingSupport support) noexcept {
    support_ = support;

    if (support_.presentStageQueries & vk::PresentStageFlagBitsEXT::eImageFirstPixelVisible) {
        presentStage_ = vk::PresentStageFlagBitsEXT::eImageFirstPixelVisible;
    } else if (support_.presentStageQueries & vk::PresentStageFlagBitsEXT::eImageFirstPixelOut) {
        presentStage_ = vk::PresentStageFlagBitsEXT::eImageFirstPixelOut;
    } else if (support_.presentStageQueries & vk::PresentStageFlagBitsEXT::eRequestDequeued) {
        presentStage_ = vk::PresentStageFlagBitsEXT::eRequestDequeued;
    } else {
        presentStage_ = {};
    }

    auto& log = platform::LogLocator::get();

    if (support_.capabilitiesKnown && !support_.presentTiming) {
        log.write(platform::LogLevel::Warning,
                  "[renderer] This VkSurfaceKHR does not support present timing; frame pacing via "
                  "VK_EXT_present_timing is unavailable");
    } else if (support_.capabilitiesKnown && !support_.presentAtRelative) {
        log.write(platform::LogLevel::Warning,
                  support_.presentAtAbsolute
                      ? "[renderer] The surface supports present timing, but only presentAtAbsoluteTime; "
                        "frame pacing via present-at-relative-time is unavailable (absolute-time "
                        "presentation is not implemented)"
                      : "[renderer] The surface supports present timing queries but neither "
                        "presentAtRelativeTime nor presentAtAbsoluteTime; no present-at-* request will "
                        "take effect");
    }
}

void Renderer::PresentTimingController::queryTimingProperties(vk::Device device, vk::SwapchainKHR swapchain) {
    if(!pfnGetSwapchainTimingProperties_) {
        pfnGetSwapchainTimingProperties_ = reinterpret_cast<PFN_vkGetSwapchainTimingPropertiesEXT>(
            vkGetDeviceProcAddr(device, "vkGetSwapchainTimingPropertiesEXT")
        );
        if(!pfnGetSwapchainTimingProperties_) {
            platform::LogLocator::get().write(platform::LogLevel::Warning, "[renderer] Attempt to obtain 'PFN_vkGetSwapchainTimingPropertiesEXT' pointer failed, so refresh mode remains #UNKNOWN");
        }
    }
    if(pfnGetSwapchainTimingProperties_) {
        vk::SwapchainTimingPropertiesEXT props{};
        uint64_t counter = 0;
        auto result = pfnGetSwapchainTimingProperties_(device, swapchain, props, &counter);
        if(result != VK_SUCCESS || props.refreshDuration == 0) {
            mode_ = RefreshMode::UNKNOWN;
            platform::LogLocator::get().write(platform::LogLevel::Info, "[renderer] Display Refresh Mode: #UNKNOWN");
            return;
        }
        duration_ = props.refreshDuration;
        interval_ = props.refreshInterval;
        if(interval_ == UINT64_MAX) {
            mode_ = RefreshMode::VRR;
            platform::LogLocator::get().write(platform::LogLevel::Info, "[renderer] Display Refresh Mode: VRR");
        }
        else if( duration_ == interval_) {
            mode_ = RefreshMode::FRR;
            platform::LogLocator::get().write(platform::LogLevel::Info, "[renderer] Display Refresh Mode: FRR");
        }
        else {
            mode_ = RefreshMode::ARR;
            platform::LogLocator::get().write(platform::LogLevel::Info, "[renderer] Display Refresh Mode: ARR");
        }
        return;
    }
    mode_ = RefreshMode::UNKNOWN;
    platform::LogLocator::get().write(platform::LogLevel::Info, "[renderer] Display Refresh Mode: #UNKNOWN");
}

void Renderer::PresentTimingController::queryTimeDomains(vk::Device device, vk::SwapchainKHR swapchain) {
    pfnGetSwapchainTimeDomainProperties_ = reinterpret_cast<PFN_vkGetSwapchainTimeDomainPropertiesEXT>(
        vkGetDeviceProcAddr(device, "vkGetSwapchainTimeDomainPropertiesEXT")
    );
    if(!pfnGetSwapchainTimeDomainProperties_) {
        mode_ = RefreshMode::UNKNOWN;
        platform::LogLocator::get().write(platform::LogLevel::Warning, "[renderer] Attempt to obtain 'PFN_vkGetSwapchainTimeDomainPropertiesEXT' pointer failed, so refresh mode remains #UNKNOWN");
        return;
    }

    uint64_t count = 0;
    VkSwapchainTimeDomainPropertiesEXT props{};
    props.pTimeDomains = nullptr; props.pTimeDomainIds = nullptr;

    VkResult result = pfnGetSwapchainTimeDomainProperties_(device, swapchain, &props, &count);
    if(result != VK_SUCCESS || count == 0) {
        platform::LogLocator::get().write(platform::LogLevel::Info, "[renderer] Unsupport any time domain, so refresh mode remains #UNKNOWN");
        mode_ = RefreshMode::UNKNOWN;
        return;
    }
    timeDomainIds_.resize(count);
    timeDomains_.resize(count);
    props.timeDomainCount = count;
    props.pTimeDomainIds = timeDomainIds_.data();
    props.pTimeDomains = timeDomains_.data();
    
    std::ignore = pfnGetSwapchainTimeDomainProperties_(device, swapchain, &props, nullptr);
    selectTimeDomain();
}

void Renderer::PresentTimingController::selectTimeDomain() {
    for(size_t i = 0; i < timeDomains_.size(); ++i) {
        if(timeDomains_[i] == VK_TIME_DOMAIN_PRESENT_STAGE_LOCAL_EXT) {
            timeDomainId_ = timeDomainIds_[i];
            return;
        }
    }
    mode_ = RefreshMode::UNKNOWN;
    platform::LogLocator::get().write(platform::LogLevel::Info, "[renderer] could not find suitable time domain, so refresh mode remains #UNKNOWN");
}

uint64_t Renderer::PresentTimingController::snapToRefreshCycles(uint64_t duration) const {
    if(interval_ == 0 || duration == 0) return 0;

    // The presentation engine can only hold an image for a whole number of
    // refresh cycles, so a request always lands on a cycle boundary. Round to the
    // nearest cycle: rounding up would silently halve the frame rate (60 Hz panel,
    // 120 FPS target -> 33.3 ms = 30 FPS), and rounding down would ask for a
    // shorter visible time than the display can actually deliver.
    uint64_t cycles = 1;
    if(duration > interval_) {
        cycles = (duration + interval_ / 2) / interval_;
    }
    return cycles * interval_;
}

uint64_t Renderer::PresentTimingController::targetDuration(double targetFPS) const {
    // Without a usable timing mode and time domain there is no request to make,
    // and the caller has to fall back to a blocking present mode.
    if (mode_ == RefreshMode::UNKNOWN || targetFPS <= 0.0 || duration_ == 0) return 0;
    // Without surface support the driver ignores the request entirely.
    if (support_.capabilitiesKnown && !support_.presentTiming) return 0;

    if(mode_ == RefreshMode::VRR) {
        return computeVRRTargetTime(duration_, targetFPS);
    }
    const uint64_t targetPeriod = static_cast<uint64_t>(1e9 / targetFPS);
    return snapToRefreshCycles(targetPeriod);
}

uint64_t Renderer::PresentTimingController::computeVRRTargetTime(uint64_t D, double targetFPS) const {
    const uint64_t targetPeriod = static_cast<uint64_t>(1e9 / targetFPS);
    return (targetPeriod < D) ? D : targetPeriod;
}

Renderer::Renderer(Dependencies deps, const RenderSettings& settings)
    : window_(deps.window),
      surface_(deps.surface),
      rct_(deps.rct),
      cameras_(deps.cameras),
      frameParams_(deps.frameParams),
      graphicsPool_(deps.graphicsPool),
      rhiFactory_(deps.factory),
      settings_(settings),
      spirvPath_(deps.spirvPath),
      descriptorSets_(deps.descriptorSets),
      shaders_(deps.shaders),
      setLayoutLibrary_(std::move(deps.setLayoutLibrary)) {
    if (!setLayoutLibrary_) {
        throw std::invalid_argument("Renderer requires a DescriptorSetLayoutLibrary");
    }

    pipelineLayouts_ = std::make_unique<PipelineLayoutLibrary>(rct_, *setLayoutLibrary_);

    swapchain_ = std::make_unique<rhi::Swapchain>(rct_, deps.alloc, surface_, window_,
                                                  rhiFactory_, settings_);

    // Surface-level support has to be known before the swapchain is created (the
    // create flag depends on it); the per-swapchain timing properties come after.
    presentController.setSurfaceSupport(toSurfaceSupport(swapchain_->timingCapabilities()));

    presentController.queryTimingProperties(*rct_.device, *swapchain_->swapChain());
    presentController.queryTimeDomains(*rct_.device, *swapchain_->swapChain());
    // Set 0's layout and table come from the shared DescriptorSetLayoutLibrary.
    const std::string_view layoutPaths[]{spirvPath_};
    const LayoutSet& layouts = setLayoutLibrary_->layoutSetFor(layoutPaths);

    frames_ = std::make_unique<FrameResources>();
    frames_->init(rct_,
                  graphicsPool_,
                  deps.alloc,
                  descriptorSets_,
                  layouts.bySetIndex[0],
                  layouts.bindingTables[0],
                  static_cast<uint32_t>(swapchain_->Image_.images.size()));
    createPipeline();
}

Renderer::~Renderer() {
    if (!cleaned_) cleanup();
}

void Renderer::createPipeline() {
    GraphicsPipelineSpec spec;
    spec.colorFormat = swapchain_->getSurfaceFormat().format;
    spec.depthFormat = swapchain_->depthFormat();
    spec.msaaSamples = settings_.msaaSamples;

    // Push constant ranges come from the shader's own reflection (ShaderLibrary
    // merges them across entry points), so the layout cannot drift from the
    // shader the way a hand-written range would.
    std::vector<PushConstantRangeSpec> pushConstants;
    if (const auto& pc = shaders_.pushConstant(spirvPath_)) {
        pushConstants.emplace_back(PushConstantRangeSpec{pc->stageFlags, pc->offset, pc->size});
    }
    const std::string_view layoutPaths[]{spirvPath_};
    const PipelineLayout& layout = pipelineLayouts_->getFor(layoutPaths, pushConstants);

    pipeline_ = std::make_unique<Pipeline>(rct_, layout, shaders_, spirvPath_, spec);
    recorder_ = std::make_unique<CommandRecorder>(*swapchain_, *pipeline_, rhiFactory_);
}

void Renderer::cleanup() {
    rct_.device.waitIdle();
    swapchain_->cleanupSwapChain();
    cleaned_ = true;
    // FrameResources members release their own Vulkan handles via RAII after
    // the idle wait above.
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
        return std::nullopt;  // skip this frame
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
                      frames_->descriptorSetHandles()[ctx.frameIndex],
                      items);
}

void Renderer::endFrame(const FrameContext& ctx) {
    const uint64_t signalValue = frames_->nextSignalValue();
    std::array<vk::Semaphore, 2> signalSemaphores{
        *frames_->renderTimeline(),
        *frames_->presentWait(ctx.imageIndex)};
    std::array<uint64_t, 2> signalValues{signalValue, 0};

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

    vk::PresentInfoKHR presentInfoKHR{};
    presentInfoKHR.setWaitSemaphores(*frames_->presentWait(ctx.imageIndex))
                  .setSwapchains(*swapchain_->swapChain())
                  .setImageIndices(ctx.imageIndex);

    vk::PresentTimingInfoEXT timingInfo{};
    const auto& support = presentController.surfaceSupport();
    const uint64_t targetDuration = presentController.targetDuration(settings_.targetFPS);

    if (targetDuration != 0 && support.presentAtRelative) {
        // NEAREST_REFRESH_CYCLE lets the implementation realign a target that is a
        // hair off a refresh boundary instead of silently landing a whole cycle
        // later (a visible micro-stutter on FRR panels).
        timingInfo.setFlags(vk::PresentTimingInfoFlagBitsEXT::ePresentAtRelativeTime |
                            vk::PresentTimingInfoFlagBitsEXT::ePresentAtNearestRefreshCycle)
                  .setTargetTime(targetDuration)
                  .setTimeDomainId(presentController.timeDomainId())
                  .setPresentStageQueries({})
                  .setTargetTimeDomainPresentStage(presentController.presentStage());
        presentInfoKHR.setPNext(&timingInfo);
    }
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
    frameCursor_   = 0;
    swapchain_->recreateSwapChain(surface_, window_);
    // Surface support is a property of the surface, not of the swapchain, but
    // recreateSwapChain re-queried it against the current window system state.
    presentController.setSurfaceSupport(toSurfaceSupport(swapchain_->timingCapabilities()));
    presentController.queryTimingProperties(*rct_.device, *swapchain_->swapChain());
    presentController.queryTimeDomains(*rct_.device, *swapchain_->swapChain());
    frames_->recreateSync(static_cast<uint32_t>(swapchain_->Image_.images.size()));
}

void Renderer::fillUniformBuffer(uint32_t frame) {
    const scene::Camera& camera = cameras_.active();
    UniformBufferObject ubo{};
    ubo.view = camera.viewMatrix();
    ubo.proj = camera.projectionMatrix(
        static_cast<float>(swapchain_->getExtent().width) /
        static_cast<float>(swapchain_->getExtent().height));
    ubo.proj[1][1] *= -1;

    ubo.camPos = glm::vec4(camera.position(), 1.0f);
    ubo.light  = frameParams_.light;

    std::memcpy(frames_->uniformBuffer(frame).mappedData(), &ubo, sizeof(ubo));
}

void Renderer::writeFrameSet(uint32_t frame) {
    vk::DescriptorBufferInfo bufferInfo{};
    bufferInfo.setBuffer(frames_->uniformBuffer(frame).getHandle())
              .setOffset(0)
              .setRange(sizeof(UniformBufferObject));

    vk::WriteDescriptorSet write{};
    write.setDstSet(frames_->descriptorSetHandles()[frame])
         .setDstBinding(0)
         .setDescriptorType(vk::DescriptorType::eUniformBuffer)
         .setBufferInfo(bufferInfo);
    rct_.device.updateDescriptorSets(write, {});
}

}  // namespace render
