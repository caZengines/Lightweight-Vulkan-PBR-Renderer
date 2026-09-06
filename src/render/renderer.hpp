#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#define VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "render/frame_uniforms.hpp"
#include "render/render_item.hpp"
#include "render/render_settings.hpp"
#include "render_context.hpp"
#include "rhi/vma_allocator.hpp"

namespace platform {
class Window;
}  // namespace platform

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
class PipelineCache;
class ShaderManager;

// Fills one frame: acquire → [app records] → submit/present.
// Phase 3 made this class an orchestrator: per-frame state lives in
// FrameResources, drawing lives in CommandRecorder, pipelines are built via
// PipelineCache from a GraphicsPipelineSpec.
class Renderer final {
public:
    struct FrameContext {
        uint32_t imageIndex = 0;  // swapchain image acquired this frame
        uint32_t frameIndex = 0;  // frame-in-flight slot (UBO / set / cmd)
    };

    struct Dependencies {
        RenderContext&                       rct;
        VmaAllocator                         alloc;
        std::vector<vk::DescriptorSetLayout> setLayouts;      // [0] per-frame, [1+] per-material
        const vk::DescriptorPool&            set0Pool;
        rhi::CommandPool&                         graphicsPool;
        scene::CameraManager&                cameras;         // active() is read each frame
        FrameParams                          frameParams;     // light from the content layer
        const vk::raii::SurfaceKHR&          surface;
        platform::Window&                    window;
        std::string_view                     spirvPath;       // absolute, from app::Config
        const rhi::RhiFactory&               factory;
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
    void createPipeline();
    void fillUniformBuffer(uint32_t frame);
    void writeFrameSet(uint32_t frame);
    void recreateAfterResize();

    platform::Window&             window_;
    const vk::raii::SurfaceKHR&   surface_;
    RenderContext                 rct_;
    scene::CameraManager&         cameras_;
    FrameParams                   frameParams_;
    rhi::CommandPool&             graphicsPool_;
    const rhi::RhiFactory&        rhiFactory_;
    RenderSettings                settings_;
    std::string_view              spirvPath_;
    std::vector<vk::DescriptorSetLayout> setLayouts_;

    std::unique_ptr<rhi::Swapchain>  swapchain_;
    std::unique_ptr<FrameResources>  frames_;
    std::unique_ptr<ShaderManager>   shaders_;
    std::unique_ptr<PipelineCache>   pipelineCache_;
    std::unique_ptr<CommandRecorder> recorder_;

    uint32_t frameCursor_ = 0;   // mirrors legacy frameIndex semantics
    bool          cleaned_     = false;
};

}  // namespace render
