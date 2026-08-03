#include "resourcefactory.hpp"
#include "vma_allocator.hpp"
#include "vulkan/vulkan.hpp"
#include "renderer/swapchain.hpp"
#include <algorithm>
#include <cassert>

#define VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

Swapchain::Swapchain(RenderContext& rct, VmaAllocator* alloc, vk::raii::SurfaceKHR& surface, GLFWwindow* window)
    : rct_(rct), allocator_(alloc)
{
    createSwapChain(surface, window);
    createImageViews();
    createColorAndDepthResources(allocator_, rct_.msaaSamples);
}

void Swapchain::createSwapChain(vk::raii::SurfaceKHR& surface, GLFWwindow* window) {
    vk::SurfaceCapabilitiesKHR caps = rct_.physicalDevice.getSurfaceCapabilitiesKHR(*surface);
    extent        = chooseExtent(caps, window);
    uint32_t minCount = chooseMinImageCount(caps);

    auto availableFormats = rct_.physicalDevice.getSurfaceFormatsKHR(*surface);
    surfaceformat = chooseFormat(availableFormats);

    auto availablePresent = rct_.physicalDevice.getSurfacePresentModesKHR(*surface);
    vk::PresentModeKHR presentMode = choosePresentMode(availablePresent);

    vk::SwapchainCreateInfoKHR ci{};
    ci.setSurface(*surface)
      .setMinImageCount(minCount)
      .setImageFormat(surfaceformat.format)
      .setImageColorSpace(surfaceformat.colorSpace)
      .setImageExtent(extent)
      .setImageArrayLayers(1)
      .setImageUsage(vk::ImageUsageFlagBits::eColorAttachment)
      .setImageSharingMode(vk::SharingMode::eExclusive)
      .setPreTransform(caps.currentTransform)
      .setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
      .setPresentMode(presentMode)
      .setClipped(true)
      .setOldSwapchain(nullptr);

    swapChain_ = vk::raii::SwapchainKHR(rct_.device, ci);
    Image_.images    = swapChain_.getImages();
}

vk::Extent2D Swapchain::chooseExtent(vk::SurfaceCapabilitiesKHR const& caps, GLFWwindow* window) {
    if (caps.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return caps.currentExtent;
    }
    int w = 0, h = 0;
    glfwGetFramebufferSize(window, &w, &h);
    return {
        std::clamp<uint32_t>(w, caps.minImageExtent.width,  caps.maxImageExtent.width),
        std::clamp<uint32_t>(h, caps.minImageExtent.height, caps.maxImageExtent.height),
    };
}

void Swapchain::createImageViews() {
    assert(Image_.ImageViews.empty());
    auto& factory = ResourceFactory::get();
    Image_.ImageViews.reserve(Image_.images.size());
    for (auto const& image : Image_.images) {
        vk::ImageViewCreateInfo viewInfo{};
        viewInfo.setImage(image)
                .setFormat(surfaceformat.format)
                .setViewType(vk::ImageViewType::e2D)
                .setSubresourceRange({vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});
        Image_.ImageViews.emplace_back(vk::raii::ImageView(rct_.device, viewInfo));
    }
}

void Swapchain::createColorAndDepthResources(VmaAllocator* alloc, vk::SampleCountFlagBits msaaSamples) {
    auto& factory = ResourceFactory::get();
    //create ColorResource
    vk::Format colorFormat = Swapchain::getSurfaceFormat().format;
    vk::ImageCreateInfo colorCI{};
    colorCI.setExtent({Swapchain::getExtent().width, Swapchain::getExtent().height, 1})
           .setFormat(colorFormat)
           .setMipLevels(1).setSamples(msaaSamples)
           .setTiling(vk::ImageTiling::eOptimal)
           .setUsage(vk::ImageUsageFlagBits::eTransientAttachment | vk::ImageUsageFlagBits::eColorAttachment)
           .setImageType(vk::ImageType::e2D)
           .setArrayLayers(1);
    VmaAllocationCreateInfo allocCI{};
    allocCI.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    colorImage_ = VmaImage(alloc, static_cast<const VkImageCreateInfo&>(colorCI), allocCI);
    colorImageView = factory.createImageView(colorImage_, colorFormat, vk::ImageAspectFlagBits::eColor, 1);
    //create DepthResource
    vk::Format depthFormat = factory.findSupportedFormat(
            {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint},
            vk::ImageTiling::eOptimal,
            vk::FormatFeatureFlagBits::eDepthStencilAttachment
    );
    vk::ImageCreateInfo depthCI{};
    depthCI.setExtent({Swapchain::getExtent().width, Swapchain::getExtent().height, 1})
           .setFormat(depthFormat)
           .setMipLevels(1).setSamples(msaaSamples)
           .setTiling(vk::ImageTiling::eOptimal)
           .setUsage(vk::ImageUsageFlagBits::eDepthStencilAttachment)
           .setImageType(vk::ImageType::e2D)
           .setArrayLayers(1);
    depthImage_ = VmaImage(alloc, static_cast<const VkImageCreateInfo&>(depthCI), allocCI);
    depthImageView = factory.createImageView(depthImage_, depthFormat, vk::ImageAspectFlagBits::eDepth, 1);
}

void Swapchain::recreateSwapChain(vk::raii::SurfaceKHR& surface, GLFWwindow* window) {
    //Minimize the current window detection
    int w = 0, h = 0;
    glfwGetFramebufferSize(window, &w, &h);
    while (w == 0 && h == 0) {
        glfwGetFramebufferSize(window, &w, &h);
        glfwWaitEvents();
    }
    
    rct_.device.waitIdle();
    cleanupSwapChain();
    createSwapChain(surface, window);
    createImageViews();
    createColorAndDepthResources(allocator_, rct_.msaaSamples);
}

void Swapchain::cleanupSwapChain() {
    Image_.ImageViews.clear();
    swapChain_ = nullptr;
}

uint32_t Swapchain::chooseMinImageCount(vk::SurfaceCapabilitiesKHR const& caps) {
    uint32_t count = std::max(3u, caps.minImageCount);
    if (caps.maxImageCount > 0 && caps.maxImageCount < count)
        count = caps.maxImageCount;
    return count;
}

vk::SurfaceFormatKHR Swapchain::chooseFormat(const std::vector<vk::SurfaceFormatKHR>& available) {
    auto it = std::ranges::find_if(available, [](auto const& f) {
        return f.format == vk::Format::eB8G8R8A8Srgb
            && f.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
    });
    return it != available.end() ? *it : available[0];
}

vk::PresentModeKHR Swapchain::choosePresentMode(std::vector<vk::PresentModeKHR> const& available) {
    for (auto m : available)
        if (m == vk::PresentModeKHR::eMailbox) return m;
    return vk::PresentModeKHR::eFifo; 
}
