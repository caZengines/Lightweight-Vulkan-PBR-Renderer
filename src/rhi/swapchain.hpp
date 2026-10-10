#pragma once

#include <cstdint>
#include <vector>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "render/render_settings.hpp"
#include "rhi/vma_allocator.hpp"

namespace platform {
class Window;
}  // namespace platform

struct RenderContext;

namespace rhi {

// Forward declarations within the rhi namespace
class RhiFactory;

// What the *surface* (not just the physical device) can actually do with

struct SurfaceTimingCapabilities {
    bool                       queried                      = false;
    bool                       presentTimingSupported       = false;
    bool                       presentAtAbsoluteTimeSupport = false;
    bool                       presentAtRelativeTimeSupport = false;
    vk::PresentStageFlagsEXT   presentStageQueries          = {};
};

// Swapchain + MSAA color resolve target + depth attachment.
// Image-view creation and depth-format probing go through the injected
// rhi::RhiFactory; present-mode preference and MSAA sample count come from
// render::RenderSettings.
class Swapchain final {
public:
    explicit Swapchain(RenderContext& rct,
                       VmaAllocator alloc,
                       const vk::raii::SurfaceKHR& surface,
                       platform::Window& window,
                       const rhi::RhiFactory& factory,
                       const render::RenderSettings& settings);

    void recreateSwapChain(const vk::raii::SurfaceKHR& surface, platform::Window& window);
    void cleanupSwapChain();

    ~Swapchain() = default;

    struct Images {
        std::vector<vk::Image>           images;
        std::vector<vk::raii::ImageView> views;
    } Image_;

    [[nodiscard]] vk::Extent2D              getExtent()        const { return extent_; }
    [[nodiscard]] vk::SurfaceFormatKHR      getSurfaceFormat() const { return surfaceFormat_; }
    [[nodiscard]] vk::raii::SwapchainKHR&   swapChain()              { return swapChain_; }

    // Present-timing capabilities of the surface this swapchain was built on.
    // Queried once per swapchain creation (surface support never changes).
    [[nodiscard]] const SurfaceTimingCapabilities& timingCapabilities() const {
        return timingCaps_;
    }

    // Format of the depth attachment created alongside the swapchain — the
    // pipeline spec must match it exactly.
    [[nodiscard]] vk::Format                depthFormat()      const { return depthFormat_; }

    [[nodiscard]] const VkImage               getcolorImage()     const { return colorImage_.getHandle(); }
    [[nodiscard]] const VkImage               getDepthImage()     const { return depthImage_.getHandle(); }
    [[nodiscard]] const vk::raii::ImageView&  getColorImageView() const { return colorImageView_; }
    [[nodiscard]] const vk::raii::ImageView&  getDepthImageView() const { return depthImageView_; }

private:
    using Capabilities = vk::SurfaceCapabilitiesKHR;

    static vk::Extent2D         chooseExtent(const Capabilities&, platform::Window&);
    static uint32_t             chooseMinImageCount(const Capabilities&);
    static vk::SurfaceFormatKHR chooseFormat(const std::vector<vk::SurfaceFormatKHR>&);
    static vk::PresentModeKHR   choosePresentMode(std::vector<vk::PresentModeKHR> const& available,
                                               vk::PresentModeKHR preferred);

    void createSwapChain(const vk::raii::SurfaceKHR& surface, platform::Window& window);
    void querySurfaceTimingCapabilities(const vk::raii::SurfaceKHR& surface);
    void createImageViews();
    void createColorAndDepthResources();

    RenderContext&          rct_;
    VmaAllocator            allocator_;
    const rhi::RhiFactory&  factory_;
    render::RenderSettings  settings_;

    vk::raii::SwapchainKHR  swapChain_     = nullptr;
    vk::SurfaceFormatKHR    surfaceFormat_{};
    vk::Extent2D            extent_{};
    vk::Format              depthFormat_   = vk::Format::eUndefined;

    SurfaceTimingCapabilities timingCaps_{};

    rhi::VmaImage           colorImage_;
    vk::raii::ImageView     colorImageView_ = nullptr;
    rhi::VmaImage           depthImage_;
    vk::raii::ImageView     depthImageView_ = nullptr;
};

}  // namespace rhi
