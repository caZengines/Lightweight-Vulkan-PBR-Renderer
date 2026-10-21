#include "render/renderer.hpp"
#include "platform/log.hpp"
#include "platform/cpu.h"


#include <array>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

#define VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>
#include "vulkan/vulkan.hpp"

#include "rhi/command/command_pool.hpp"
#include "rhi/present/swapchain.hpp"
#include "render/pass/command_recorder.hpp"
#include "render/descriptor/descriptor_set_layout_library.hpp"
#include "render/frame/frame_resources.hpp"
#include "render/frame/frame_uniforms.hpp"
#include "render/pass/pass.hpp"
#include "render/pass/raster_pass.hpp"
#include "render/compute/compute_pass.hpp"
#include "render/compute/compute_pipeline_library.hpp"
#include "render/compute/compute_pipeline_spec.hpp"
#include "render/pipeline/graphics_pipeline_library.hpp"
#include "render/pipeline/graphics_pipeline_spec.hpp"
#include "render/pipeline/pipeline_layout_library.hpp"
#include "resource/gpu/shader_library.hpp"
#include "rhi/device/render_context.hpp"
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

// First stage that consumes the acquired swapchain image.
constexpr vk::PipelineStageFlags kPresentWaitStage =
    vk::PipelineStageFlagBits::eColorAttachmentOutput;

}  // namespace

// ============================================================
// ----------------- PresentTimingController ------------------
// ============================================================

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
    } else if (support_.capabilitiesKnown && presentStage_ == vk::PresentStageFlagsEXT{}) {
        log.write(platform::LogLevel::Warning,
                  "[renderer] The surface exposes no display-near present stage (only end-of-queue "
                  "operations); present-stage timestamps are unavailable, so PresentTimingController disable! ");
    }
}

void Renderer::PresentTimingController::queryTimingProperties(vk::Device device, vk::SwapchainKHR swapchain) {
    if(!pfnGetSwapchainTimingProperties_) {
        pfnGetSwapchainTimingProperties_ = reinterpret_cast<PFN_vkGetSwapchainTimingPropertiesEXT>(
            vkGetDeviceProcAddr(device, "vkGetSwapchainTimingPropertiesEXT")
        );
        if(!pfnGetSwapchainTimingProperties_) {
            platform::LogLocator::get().write(platform::LogLevel::Warning, "[renderer] Attempt to obtain 'PFN_vkGetSwapchainTimingPropertiesEXT' pointer failed, so refresh mode remains #UNKNOWN");
            return;
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
    }
    // The pacer needs present-stage timestamps: vkGetCalibratedTimestampsKHR with
    // a VkSwapchainCalibratedTimestampInfoEXT extension, which is what turns the
    // opaque swapchain time domain into a readable moment.
    if(!pfnGetCalibrated_) {
        pfnGetCalibrated_ = reinterpret_cast<PFN_vkGetCalibratedTimestampsKHR>(
            vkGetDeviceProcAddr(device, "vkGetCalibratedTimestampsKHR")
        );
        if(!pfnGetCalibrated_) {
            platform::LogLocator::get().write(platform::LogLevel::Warning, "[renderer] Attempt to obtain 'PFN_vkGetCalibratedTimestampsKHR' pointer failed; present-stage pacing is unavailable");
        }
    }
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
    props.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_TIME_DOMAIN_PROPERTIES_EXT;
    props.pTimeDomains = nullptr; props.pTimeDomainIds = nullptr;

    VkResult result = pfnGetSwapchainTimeDomainProperties_(device, swapchain, &props, &count);
    if(result != VK_SUCCESS || count == 0) {
        platform::LogLocator::get().write(platform::LogLevel::Info, "[renderer] Unsupport any time domain, so refresh mode remains #UNKNOWN");
        mode_ = RefreshMode::UNKNOWN;
        return;
    }
    const uint32_t capacity = props.timeDomainCount;
    timeDomainIds_.resize(capacity);
    timeDomains_.resize(capacity);
    props.timeDomainCount = count;
    props.pTimeDomainIds = timeDomainIds_.data();
    props.pTimeDomains = timeDomains_.data();
    
    std::ignore = pfnGetSwapchainTimeDomainProperties_(device, swapchain, &props, &count);
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

std::optional<uint64_t> Renderer::PresentTimingController::samplePresentStageTime(
    vk::Device device, vk::SwapchainKHR swapchain) const {
    if(!pfnGetCalibrated_ || !swapchain) return std::nullopt;
    if(support_.capabilitiesKnown && !support_.presentTiming) return std::nullopt;
    if(timeDomainId_ == 0) return std::nullopt;

    vk::SwapchainCalibratedTimestampInfoEXT swapchainInfo{};
    swapchainInfo.setSwapchain(swapchain)
                 .setPresentStage(presentStage_)
                 .setTimeDomainId(timeDomainId_);

    vk::CalibratedTimestampInfoKHR info{};
    info.setPNext(&swapchainInfo)
        .setTimeDomain(vk::TimeDomainKHR::ePresentStageLocalEXT);

    uint64_t timestamp = 0;
    uint64_t maxDeviation = 0;
    const VkResult result = pfnGetCalibrated_(device, 1, info, &timestamp, &maxDeviation);
    if(result != VK_SUCCESS) return std::nullopt;
    return timestamp;
}

uint64_t Renderer::PresentTimingController::targetDuration(double targetFPS) const {
    if (mode_ == RefreshMode::UNKNOWN || targetFPS <= 0.0 || duration_ == 0) return 0;
    // Without surface support the driver ignores the request entirely.
    if (support_.capabilitiesKnown && !support_.presentTiming) return 0;
    targetPeriod_ = static_cast<uint64_t>(1e9 / targetFPS);
    if(mode_ == RefreshMode::VRR) {
        return computeVRRTargetTime(duration_);
    }
    // Under FRR the request is a per-frame *minimum visible time*, so rounding it
    // to whole refresh cycles silently becomes a rate cap (144 fps on a 300 Hz
    // panel rounds to 3 cycles = 100 fps, and the 2.5/1.5-cycle cases are decided
    // by a +-1 ns difference in the driver-reported refresh duration). The CPU
    // pacer owns the average rate, so hand the raw period to the presentation
    // engine and let it align that to the nearest vblank.
    return targetPeriod_;
}

uint64_t Renderer::PresentTimingController::computeVRRTargetTime(uint64_t duration) const {
    return (targetPeriod_ < duration) ? duration : targetPeriod_;
}

// =================================================================
// ----------------------  renderer  -------------------------------
// =================================================================

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
      computeSpirvPath_(deps.computeSpirvPath),
      descriptorSets_(deps.descriptorSets),
      shaders_(deps.shaders),
      setLayoutLibrary_(std::move(deps.setLayoutLibrary)) {
    if (!setLayoutLibrary_) {
        throw std::invalid_argument("Renderer requires a DescriptorSetLayoutLibrary");
    }

    pipelineLayouts_ = std::make_unique<PipelineLayoutLibrary>(rct_, *setLayoutLibrary_);

    swapchain_ = std::make_unique<rhi::Swapchain>(rct_, deps.alloc, surface_, window_,
                                                  rhiFactory_, settings_);

    presentController.setSurfaceSupport(toSurfaceSupport(swapchain_->timingCapabilities()));

    presentController.queryTimingProperties(*rct_.device, *swapchain_->swapChain());
    presentController.queryTimeDomains(*rct_.device, *swapchain_->swapChain());
    // Set 0's layout and table come from the shared DescriptorSetLayoutLibrary.
    const std::array<std::string_view, 2> pathStorage{spirvPath_, computeSpirvPath_};
    const std::span<const std::string_view> layoutPaths{
        pathStorage.data(), computeSpirvPath_.empty() ? 1u : 2u};
    const LayoutSet& layouts = setLayoutLibrary_->layoutSetFor(layoutPaths);

    frames_ = std::make_unique<FrameResources>();
    frames_->init(rct_,
                  graphicsPool_,
                  deps.alloc,
                  descriptorSets_,
                  layouts.bySetIndex[0],
                  layouts.bindingTables[0],
                  static_cast<uint32_t>(swapchain_->Image_.images.size()));
    createPipelines();
}

Renderer::~Renderer() {
    if (!cleaned_) cleanup();
}

void Renderer::createPipelines() {
    const std::array<std::string_view, 2> pathStorage{spirvPath_, computeSpirvPath_};
    const std::span<const std::string_view> layoutPaths{
        pathStorage.data(), computeSpirvPath_.empty() ? 1u : 2u};

    std::vector<PushConstantRangeSpec> graphicsPushConstants;
    if (const auto& pc = shaders_.pushConstant(spirvPath_)) {
        graphicsPushConstants.emplace_back(
            PushConstantRangeSpec{pc->stageFlags, pc->offset, pc->size});
    }

    std::vector<PushConstantRangeSpec> computePushConstants;
    if (!computeSpirvPath_.empty()) {
        if (const auto& pc = shaders_.pushConstant(computeSpirvPath_)) {
            computePushConstants.emplace_back(
                PushConstantRangeSpec{pc->stageFlags, pc->offset, pc->size});
        }
    }

    const PipelineLayout& graphicsLayout =
        pipelineLayouts_->getFor(layoutPaths, graphicsPushConstants);
    graphicsLayout_ = &graphicsLayout;

    graphicsPipelines_ = std::make_unique<GraphicsPipelineLibrary>(rct_.device, shaders_);
    computePipelines_  = std::make_unique<ComputePipelineLibrary>(rct_.device, shaders_);

    if (!computeSpirvPath_.empty()) {
        const PipelineLayout& computeLayout =
            pipelineLayouts_->getFor(layoutPaths, computePushConstants);
        computePipeline_ = &computePipelines_->get(
            computeLayout, computeSpirvPath_, ComputePipelineSpec{});
        platform::LogLocator::get().write(
            platform::LogLevel::Info,
            "[renderer] compute pipeline created: " + std::string(computeSpirvPath_));
    }

    GraphicsPipelineSpec initialSpec;
    initialSpec.colorFormat = swapchain_->getSurfaceFormat().format;
    initialSpec.depthFormat = swapchain_->depthFormat();
    initialSpec.msaaSamples = settings_.msaaSamples;
    rasterPipeline_ = &graphicsPipelines_->get(graphicsLayout, spirvPath_, initialSpec);
    platform::LogLocator::get().write(
        platform::LogLevel::Info,
        "[renderer] raster pipeline created: " + std::string(spirvPath_));

    recorder_ = std::make_unique<CommandRecorder>(rhiFactory_);
}

void Renderer::ensureRasterPipeline(const RenderTarget& target) {
    GraphicsPipelineSpec spec;
    spec.colorFormat = target.colorFormat;
    spec.depthFormat = target.depthFormat;
    spec.msaaSamples = target.samples;

    const GraphicsPipeline* next = &graphicsPipelines_->get(*graphicsLayout_, spirvPath_, spec);
    if (next != rasterPipeline_) {
        rasterPipeline_ = next;
        platform::LogLocator::get().write(
            platform::LogLevel::Info,
            "[renderer] raster pipeline changed: " + std::string(spirvPath_));
    }
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

    const RenderTarget target{
        .presentImage = swapchain_->Image_.images[imageIndex],
        .presentView  = *swapchain_->Image_.views[imageIndex],
        .msaaImage    = swapchain_->getcolorImage(),
        .msaaView     = *swapchain_->getColorImageView(),
        .depthImage   = swapchain_->getDepthImage(),
        .depthView    = *swapchain_->getDepthImageView(),
        .extent       = swapchain_->getExtent(),
        .colorFormat  = swapchain_->getSurfaceFormat().format,
        .depthFormat  = swapchain_->depthFormat(),
        .samples      = settings_.msaaSamples,
    };

    return FrameContext{imageIndex, frameCursor_, target};
}

void Renderer::record(FrameContext& ctx, std::span<const RenderItem> items) {
    ensureRasterPipeline(ctx.target);

    vk::raii::CommandBuffer& cmd = frames_->commandBuffer(ctx.frameIndex);
    const vk::DescriptorSet frameSet =
        frames_->descriptorSetHandles()[ctx.frameIndex];
    const PassContext passCtx{cmd, ctx.target, frameSet};

    recorder_->begin(cmd, ctx.target);

    if (computePipeline_) {
        ComputePass computePass{*computePipeline_,
                                ComputeBindings{2, nullptr},
                                vk::Extent3D{1, 1, 1}};
        computePass.record(passCtx);
    }

    const RasterBucket bucket{rasterPipeline_, items};
    RasterPass rasterPass{std::span<const RasterBucket>{&bucket, 1}};
    rasterPass.record(passCtx);

    recorder_->end(cmd, ctx.target);
}
// Pacing contract for one frame:
//   - paceBeforeSubmit() owns the *average* cadence. It spaces submissions by
//     1/targetFPS and accumulates the deadline so sleep overshoot does not leak
//     into a lower average rate; a frame that is already late re-syncs instead of
//     trying to catch up.
//   - presentController.targetDuration() only tells the presentation engine when
//     an image may become visible (VkPresentTimingsInfoEXT), so it must stay the
//     raw period: any snapping to whole refresh cycles turns into a rate cap.
void Renderer::resetCpuPacer() noexcept {
    cpuPacerPrimed_ = false;
    lastSubmit_ = {};
}
void Renderer::paceBeforeSubmit() {
    using Clock = platform::SteadyClock;

    const double targetFPS = settings_.targetFPS;
    if (!std::isfinite(targetFPS) || targetFPS <= 0.0 ||
        targetFPS > 1'000'000.0) {
        resetCpuPacer();
        return;
    }
    const auto period = std::chrono::round<Clock::duration>(
        std::chrono::duration<double>(1.0 / targetFPS)
    );
    if(period <= Clock::duration::zero()) {
        resetCpuPacer();
        return;
    }

    auto now = Clock::now();
    if(!cpuPacerPrimed_) {
        cpuPacerPrimed_ = true;
        lastSubmit_ = now;
        return;
    }

    const auto deadline = lastSubmit_ + period;
    if (deadline <= now) {
        lastSubmit_ = now;
        return;
    }
    platform::sleepUntil(deadline);
    while(Clock::now() < deadline) platform::relax();

    lastSubmit_ = deadline;
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

    const vk::SubmitInfo submitInfo = [&] {
        vk::SubmitInfo info;
        info.setWaitSemaphores(*frames_->presentComplete(ctx.frameIndex))
            .setWaitDstStageMask(kPresentWaitStage)
            .setCommandBuffers(*frames_->commandBuffer(ctx.frameIndex))
            .setSignalSemaphores(signalSemaphores)
            .setPNext(&timelineSubmitInfo);
        return info;
    }();

    paceBeforeSubmit();
    graphicsPool_.queue().submit(submitInfo, *frames_->inFlightFence(ctx.frameIndex));

    vk::PresentInfoKHR presentInfoKHR{};
    presentInfoKHR.setWaitSemaphores(*frames_->presentWait(ctx.imageIndex))
                  .setSwapchains(*swapchain_->swapChain())
                  .setImageIndices(ctx.imageIndex);

    vk::PresentTimingInfoEXT  timingInfo{};
    vk::PresentTimingsInfoEXT timingsInfo{};
    const auto& support = presentController.surfaceSupport();
    const uint64_t targetDuration = presentController.targetDuration(settings_.targetFPS);

    if (targetDuration != 0 && support.presentAtRelative) {
        timingInfo.setFlags(vk::PresentTimingInfoFlagBitsEXT::ePresentAtRelativeTime |
                            vk::PresentTimingInfoFlagBitsEXT::ePresentAtNearestRefreshCycle)
                  .setTargetTime(targetDuration)
                  .setTimeDomainId(presentController.timeDomainId())
                  .setTargetTimeDomainPresentStage(presentController.presentStage());
        timingsInfo.setSwapchainCount(1).setPTimingInfos(&timingInfo);
        presentInfoKHR.setPNext(&timingsInfo);
    }
    auto result = graphicsPool_.queue().presentKHR(presentInfoKHR);
    if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR ||
        framebufferResized) {
        framebufferResized = false;
        recreateAfterResize();
    }
    else assert(result == vk::Result::eSuccess);

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
