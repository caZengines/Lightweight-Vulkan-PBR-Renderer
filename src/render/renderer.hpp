#pragma once

#include "render/pass/raster_pass.hpp"
#include <array>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>
#include <span>
#include <string_view>


#include <vulkan/vulkan_raii.hpp>

#include "render/frame/frame_uniforms.hpp"
#include "render/frame/render_item.hpp"
#include "render/frame/render_settings.hpp"
#include "render/frame/render_target.hpp"
#include "render/compute/compute_resources.hpp"
#include "render/material/material_pipeline_key.hpp"
#include "rhi/device/render_context.hpp"
#include "rhi/descriptor/descriptor_set_allocator.hpp"
#include "rhi/memory/vma_allocator.hpp"

namespace platform {
class Window;
}  // namespace platform

namespace resource {
class ShaderLibrary;
class UploadQueue;
}  // namespace resource

namespace rhi {
class CommandPool;
class RhiFactory;
class Swapchain;
}  // namespace rhi

namespace scene {
class CameraManager;
}  // namespace scene

namespace render {

class CommandRecorder;
class FrameResources;
class DescriptorSetLayoutLibrary;
class PipelineLayout;
class PipelineLayoutLibrary;
class GraphicsPipeline;
class GraphicsPipelineLibrary;
class ComputePipeline;
class ComputePipelineLibrary;
class LayoutSet;

// Present-timing support as reported by
// vkGetPhysicalDeviceSurfaceCapabilities2KHR for the window's surface. Device
// features only say the implementation *could*; this says whether the surface in
// front of us actually will honor a present-at-* request.
struct SurfaceTimingSupport {
    bool                     presentTiming       = false;
    bool                     presentAtAbsolute   = false;
    bool                     presentAtRelative   = false;
    vk::PresentStageFlagsEXT presentStageQueries = {};
    bool                     capabilitiesKnown   = false;  // surface query answered at all
};

// Fills one frame: acquire -> [app records] -> submit/present.
// Orchestration only: per-frame state lives in FrameResources, drawing lives in
// RasterPass/ComputePass, and pipeline objects are cached per family in
// GraphicsPipelineLibrary / ComputePipelineLibrary.
class Renderer final {
public:
    struct FrameContext {
        uint32_t     imageIndex = 0;  // swapchain image acquired this frame
        uint32_t     frameIndex = 0;  // frame-in-flight slot (UBO / set / cmd)
        RenderTarget target{};
    };

    struct Dependencies {
        RenderContext&                              rct;
        VmaAllocator                                alloc;

        std::unique_ptr<DescriptorSetLayoutLibrary> setLayoutLibrary;
        rhi::DescriptorSetAllocator&                descriptorSets;  // app-owned, must outlive us
        const resource::ShaderLibrary&              shaders;         // app-owned
        rhi::CommandPool&                           graphicsPool;
        resource::UploadQueue&                      queue;
        scene::CameraManager&                       cameras;         // active() is read each frame
        FrameParams                                 frameParams;     // light from the content layer
        const vk::raii::SurfaceKHR&                 surface;
        platform::Window&                           window;
        std::string_view                            spirvPath;       // absolute, from app::Config
        std::string_view                            computeSpirvPath;  // absolute, from app::Config
        const rhi::RhiFactory&                      factory;
    };

    explicit Renderer(Dependencies deps, const RenderSettings& settings);
    ~Renderer();

    bool framebufferResized = false;   // written by the app's window-resize hook

    // Waits for the slot fence and acquires a swapchain image; returns nullopt
    // when the frame is skipped because the swapchain is being recreated.
    [[nodiscard]] std::optional<FrameContext> beginFrame();

    void record(FrameContext& ctx, std::span<const RenderItem> items);
    void endFrame(const FrameContext& ctx);

    void cleanup();

private:
    void createPipelines();
    void ensureRasterPipelines(const RenderTarget& target);
    [[nodiscard]] const GraphicsPipeline* rasterPipelineFor(
        MaterialPipelineKey key, bool blend, const RenderTarget& target);
    void fillUniformBuffer(uint32_t frame);
    void writeFrameSet(uint32_t frame);
    void recreateAfterResize();

    platform::Window&             window_;
    const vk::raii::SurfaceKHR&   surface_;
    RenderContext                 rct_;
    scene::CameraManager&         cameras_;
    FrameParams                   frameParams_;
    rhi::CommandPool&             graphicsPool_;
    resource::UploadQueue&        queue_;
    const rhi::RhiFactory&        rhiFactory_;
    RenderSettings                settings_;
    std::string_view              spirvPath_;
    std::string_view              computeSpirvPath_;
    rhi::DescriptorSetAllocator&  descriptorSets_;
    const resource::ShaderLibrary& shaders_;

    std::unique_ptr<DescriptorSetLayoutLibrary> setLayoutLibrary_;
    std::unique_ptr<PipelineLayoutLibrary>      pipelineLayouts_;
    std::unique_ptr<GraphicsPipelineLibrary>    graphicsPipelines_;
    std::unique_ptr<ComputePipelineLibrary>     computePipelines_;
    std::unique_ptr<rhi::Swapchain>             swapchain_;
    std::unique_ptr<FrameResources>             frames_;
    std::unique_ptr<ComputeResources>           computeResources_;
    std::unique_ptr<CommandRecorder>            recorder_;

    struct RasterVariants {
        std::array<std::array<const GraphicsPipeline*, kMaterialPipelineVariantCount>, 2> pipelines{};
        vk::Format colorFormat = vk::Format::eUndefined;
        vk::Format depthFormat = vk::Format::eUndefined;
        vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1;
    };

    const LayoutSet*          layouts_         = nullptr;
    const PipelineLayout*     graphicsLayout_  = nullptr;
    RasterVariants            rasterVariants_{};
    std::vector<RasterBucket> rasterBuckets_;
    // Blend items copied out of the scene's immutable draw list so they can be
    // depth-sorted per frame; rasterBuckets_ views into this storage.
    std::vector<RenderItem>   blendDrawItems_;
    const GraphicsPipeline*   particlePipeline_ = nullptr;
    const ComputePipeline*    computePipeline_ = nullptr;

    uint32_t                frameCursor_     = 0;  // frame-in-flight slot for this frame
    bool                    cleaned_         = false;

    std::chrono::steady_clock::time_point       lastSubmit_{};
    bool                                        cpuPacerPrimed_ = false;

    void paceBeforeSubmit();
    void resetCpuPacer() noexcept;

    enum class RefreshMode : uint8_t {
        UNKNOWN = 0,
        FRR, // Fixed Refresh rate
        VRR, // variable Refresh rate
        ARR  // Adaptive Refresh Rate
    };

    // Owns the frame-pacing state that VK_EXT_present_timing exposes for one
    // swapchain.
    class PresentTimingController final {
        public:
            // Marks the surface capabilities obtained from
            // vkGetPhysicalDeviceSurfaceCapabilities2KHR.
            void setSurfaceSupport(SurfaceTimingSupport support) noexcept;

            void queryTimingProperties(vk::Device device, vk::SwapchainKHR swapchain);
            void queryTimeDomains(vk::Device device, vk::SwapchainKHR swapchain);

            [[nodiscard]] std::optional<uint64_t> samplePresentStageTime(vk::Device device,
                                                                        vk::SwapchainKHR swapchain) const;

            // Frame period (ns) to hand to VkPresentTimingsInfoEXT for targetFPS,
            // 0 = send no timing request (FIFO/the CPU pacer alone limit the rate).
            // Returns the raw 1e9/targetFPS under FRR: the value is a per-frame
            // minimum visible time, so snapping it to whole refresh cycles would
            // cap the rate below the target (144 fps on 300 Hz -> 3 cycles -> 100).
            // Rate ownership: paceBeforeSubmit() sets the average cadence, this
            // value only tells the presentation engine how to align to vblank.
            [[nodiscard]] uint64_t targetDuration(double targetFPS) const;

            [[nodiscard]] RefreshMode mode() const noexcept { return mode_; }
            [[nodiscard]] uint64_t timeDomainId() const noexcept { return timeDomainId_; }
            [[nodiscard]] vk::PresentStageFlagsEXT presentStage() const noexcept { return presentStage_; }
            [[nodiscard]] const SurfaceTimingSupport& surfaceSupport() const noexcept { return support_; }


        private:
            PFN_vkGetSwapchainTimingPropertiesEXT     pfnGetSwapchainTimingProperties_     = nullptr;
            PFN_vkGetSwapchainTimeDomainPropertiesEXT pfnGetSwapchainTimeDomainProperties_ = nullptr;
            PFN_vkGetCalibratedTimestampsKHR          pfnGetCalibrated_                    = nullptr;
            RefreshMode mode_ = RefreshMode::UNKNOWN;
            mutable uint64_t targetPeriod_ = 0;
            uint64_t duration_     = 0;   // one refresh cycle, ns
            uint64_t interval_     = 0;   // adjustable step, ns (UINT64_MAX under VRR)

            std::vector<VkTimeDomainKHR> timeDomains_;
            std::vector<uint64_t> timeDomainIds_;
            uint64_t timeDomainId_    = 0;

            SurfaceTimingSupport support_{};
            // Present stage the timestamp is sampled at, chosen from
            // presentStageQueries. 0 = this surface exposes no display-near
            // stage, so samplePresentStageTime() cannot be used.
            vk::PresentStageFlagsEXT presentStage_ = {};

            void selectTimeDomain();
            uint64_t computeVRRTargetTime(uint64_t duration) const;
    };
    PresentTimingController presentController;
};

}  // namespace render
