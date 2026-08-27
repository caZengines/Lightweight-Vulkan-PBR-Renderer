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

namespace rhi {
class RhiFactory;
}  // namespace rhi

struct RenderContext;

namespace render {

// Swapchain + MSAA color resolve target + depth attachment.
// Phase 3: image-view creation and depth-format probing go through the
// injected rhi::RhiFactory (singleton removed); present-mode preference and
// MSAA sample count come from RenderSettings instead of hardcodes.
class Swapchain final {
public:
    explicit Swapchain(RenderContext& rct,
                       VmaAllocator alloc,
                       vk::raii::SurfaceKHR& surface,
                       platform::Window& window,
                       const rhi::RhiFactory& factory,
                       const RenderSettings& settings);

    void recreateSwapChain(vk::raii::SurfaceKHR& surface, platform::Window& window);
    void cleanupSwapChain();

    ~Swapchain() = default;

    struct Images {
        std::vector<vk::Image>           images;
        std::vector<vk::raii::ImageView> views;
    } Image_;

    [[nodiscard]] vk::Extent2D              getExtent()        const { return extent_; }
    [[nodiscard]] vk::SurfaceFormatKHR      getSurfaceFormat() const { return surfaceFormat_; }
    [[nodiscard]] vk::raii::SwapchainKHR&   swapChain()              { return swapChain_; }

    // Format of the depth attachment created alongside the swapchain — the
    // pipeline spec must match it exactly.
    [[nodiscard]] vk::Format                depthFormat()      const { return depthFormat_; }

    [[nodiscard]] const VkImage               getcolorImage()     const { return colorImage_.getHandle(); }
    [[nodiscard]] const VkImage               getDepthImage()     const { return depthImage_.getHandle(); }
    [[nodiscard]] const vk::raii::ImageView&  getColorImageView() const { return colorImageView_; }
    [[nodiscard]] const vk::raii::ImageView&  getDepthImageView() const { return depthImageView_; }

private:
    using Capabilities = vk::SurfaceCapabilitiesKHR;

    static vk::Extent2D       chooseExtent(const Capabilities&, platform::Window&);
    static std::uint32_t      chooseMinImageCount(const Capabilities&);
    static vk::SurfaceFormatKHR chooseFormat(const std::vector<vk::SurfaceFormatKHR>&);
    static vk::PresentModeKHR choosePresentMode(std::vector<vk::PresentModeKHR> const& available,
                                               vk::PresentModeKHR preferred);

    void createSwapChain(vk::raii::SurfaceKHR& surface, platform::Window& window);
    void createImageViews();
    void createColorAndDepthResources();

    RenderContext&          rct_;
    VmaAllocator            allocator_;
    const rhi::RhiFactory&  factory_;
    RenderSettings          settings_;

    vk::raii::SwapchainKHR  swapChain_     = nullptr;
    vk::SurfaceFormatKHR    surfaceFormat_{};
    vk::Extent2D            extent_{};
    vk::Format              depthFormat_   = vk::Format::eUndefined;

    rhi::VmaImage           colorImage_;
    vk::raii::ImageView     colorImageView_ = nullptr;
    rhi::VmaImage           depthImage_;
    vk::raii::ImageView     depthImageView_ = nullptr;
};

}  // namespace render
