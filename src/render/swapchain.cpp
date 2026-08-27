#include "render/swapchain.hpp"

#include <algorithm>
#include <cassert>
#include <limits>

#define VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "platform/window.hpp"
#include "rhi/rhi_factory.hpp"
#include "render_context.hpp"

namespace render {

Swapchain::Swapchain(RenderContext& rct,
                     VmaAllocator alloc,
                     vk::raii::SurfaceKHR& surface,
                     platform::Window& window,
                     const rhi::RhiFactory& factory,
                     const RenderSettings& settings)
    : rct_(rct),
      allocator_(alloc),
      factory_(factory),
      settings_(settings) {
    createSwapChain(surface, window);
    createImageViews();
    createColorAndDepthResources();
}

void Swapchain::createSwapChain(vk::raii::SurfaceKHR& surface, platform::Window& window) {
    const vk::SurfaceCapabilitiesKHR caps = rct_.physicalDevice.getSurfaceCapabilitiesKHR(*surface);
    extent_                 = chooseExtent(caps, window);
    const std::uint32_t minCount = chooseMinImageCount(caps);

    const auto availableFormats   = rct_.physicalDevice.getSurfaceFormatsKHR(*surface);
    surfaceFormat_                = chooseFormat(availableFormats);

    const auto availablePresent   = rct_.physicalDevice.getSurfacePresentModesKHR(*surface);
    const vk::PresentModeKHR presentMode =
        choosePresentMode(availablePresent, settings_.preferredPresentMode);

    vk::SwapchainCreateInfoKHR ci{};
    ci.setSurface(*surface)
      .setMinImageCount(minCount)
      .setImageFormat(surfaceFormat_.format)
      .setImageColorSpace(surfaceFormat_.colorSpace)
      .setImageExtent(extent_)
      .setImageArrayLayers(1)
      .setImageUsage(vk::ImageUsageFlagBits::eColorAttachment)
      .setImageSharingMode(vk::SharingMode::eExclusive)
      .setPreTransform(caps.currentTransform)
      .setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
      .setPresentMode(presentMode)
      .setClipped(true)
      .setOldSwapchain(nullptr);

    swapChain_    = vk::raii::SwapchainKHR(rct_.device, ci);
    Image_.images = swapChain_.getImages();
}

vk::Extent2D Swapchain::chooseExtent(const Capabilities& caps, platform::Window& window) {
    if (caps.currentExtent.width != std::numeric_limits<std::uint32_t>::max()) {
        return caps.currentExtent;
    }
    const int w = static_cast<int>(window.framebufferWidth());
    const int h = static_cast<int>(window.framebufferHeight());
    return {
        std::clamp<std::uint32_t>(static_cast<std::uint32_t>(w), caps.minImageExtent.width,
                                  caps.maxImageExtent.width),
        std::clamp<std::uint32_t>(static_cast<std::uint32_t>(h), caps.minImageExtent.height,
                                  caps.maxImageExtent.height),
    };
}

void Swapchain::createImageViews() {
    assert(Image_.views.empty());
    Image_.views.reserve(Image_.images.size());
    for (const auto image : Image_.images) {
        vk::ImageViewCreateInfo viewInfo{};
        viewInfo.setImage(image)
                .setFormat(surfaceFormat_.format)
                .setViewType(vk::ImageViewType::e2D)
                .setSubresourceRange({vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});
        Image_.views.emplace_back(rct_.device, viewInfo);
    }
}

void Swapchain::createColorAndDepthResources() {
    const vk::SampleCountFlagBits msaa = settings_.msaaSamples;

    // Color resolve target (same format as the swapchain images).
    vk::ImageCreateInfo colorCI{};
    colorCI.setExtent({extent_.width, extent_.height, 1})
           .setFormat(surfaceFormat_.format)
           .setMipLevels(1)
           .setSamples(msaa)
           .setTiling(vk::ImageTiling::eOptimal)
           .setUsage(vk::ImageUsageFlagBits::eTransientAttachment |
                     vk::ImageUsageFlagBits::eColorAttachment)
           .setImageType(vk::ImageType::e2D)
           .setArrayLayers(1);
    VmaAllocationCreateInfo allocCI{};
    allocCI.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    colorImage_      = rhi::VmaImage(allocator_, static_cast<const VkImageCreateInfo&>(colorCI), allocCI);
    colorImageView_  = factory_.createImageView(colorImage_, surfaceFormat_.format,
                                                vk::ImageAspectFlagBits::eColor);

    // Depth attachment — single probing point for the whole engine now.
    static constexpr std::array<vk::Format, 3> kDepthCandidates{
        vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint};
    depthFormat_ = factory_.findSupportedFormat(
        kDepthCandidates,
        vk::ImageTiling::eOptimal,
        vk::FormatFeatureFlagBits::eDepthStencilAttachment);

    vk::ImageCreateInfo depthCI{};
    depthCI.setExtent({extent_.width, extent_.height, 1})
           .setFormat(depthFormat_)
           .setMipLevels(1)
           .setSamples(msaa)
           .setTiling(vk::ImageTiling::eOptimal)
           .setUsage(vk::ImageUsageFlagBits::eDepthStencilAttachment)
           .setImageType(vk::ImageType::e2D)
           .setArrayLayers(1);
    depthImage_      = rhi::VmaImage(allocator_, static_cast<const VkImageCreateInfo&>(depthCI), allocCI);
    depthImageView_  = factory_.createImageView(depthImage_, depthFormat_,
                                                vk::ImageAspectFlagBits::eDepth);
}

void Swapchain::recreateSwapChain(vk::raii::SurfaceKHR& surface, platform::Window& window) {
    // Minimized-window parking loop.
    int w = static_cast<int>(window.framebufferWidth());
    int h = static_cast<int>(window.framebufferHeight());
    while (w == 0 && h == 0) {
        window.waitEvents();
        w = static_cast<int>(window.framebufferWidth());
        h = static_cast<int>(window.framebufferHeight());
    }

    rct_.device.waitIdle();
    cleanupSwapChain();
    createSwapChain(surface, window);
    createImageViews();
    createColorAndDepthResources();
}

void Swapchain::cleanupSwapChain() {
    Image_.views.clear();
    swapChain_ = nullptr;
    // color/depth VmaImages and their views release via RAII when this object
    // dies (known pre-existing trait; not silently "fixed" here).
}

std::uint32_t Swapchain::chooseMinImageCount(const Capabilities& caps) {
    std::uint32_t count = std::max(3u, caps.minImageCount);
    if (caps.maxImageCount > 0 && caps.maxImageCount < count) {
        count = caps.maxImageCount;
    }
    return count;
}

vk::SurfaceFormatKHR Swapchain::chooseFormat(const std::vector<vk::SurfaceFormatKHR>& available) {
    const auto it = std::ranges::find_if(available, [](const auto& f) {
        return f.format == vk::Format::eB8G8R8A8Srgb &&
               f.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
    });
    return it != available.end() ? *it : available.front();
}

vk::PresentModeKHR Swapchain::choosePresentMode(std::vector<vk::PresentModeKHR> const& available,
                                                vk::PresentModeKHR preferred) {
    if (std::ranges::find(available, preferred) != available.end()) {
        return preferred;
    }
    return vk::PresentModeKHR::eFifo;  // universally supported fallback
}

}  // namespace render
