#include "resourcefactory.hpp"
#include "swapchain.hpp"
#include <algorithm>
#include <cassert>

#define VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

Swapchain::Swapchain(RenderContext& rct, vk::raii::SurfaceKHR& surface, GLFWwindow* window)
    : rct_(rct)
{
    createSwapChain(surface, window);
    createImageViews();
    createColorAndDepthResources(rct_.msaaSamples);
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
        Image_.ImageViews.emplace_back(
            factory.createImageView(image, surfaceformat.format, vk::ImageAspectFlagBits::eColor, 1));
    }
}

void Swapchain::createColorAndDepthResources(vk::SampleCountFlagBits msaaSamples) {
    auto& factory = ResourceFactory::get();
    //create ColorResource
    vk::Format colorFormat = Swapchain::getSurfaceFormat().format;
    std::tie(colorImage, colorImageMemory) = 
                factory.createImage(
                    Swapchain::getExtent().width, Swapchain::getExtent().height, 1, msaaSamples, colorFormat,
                    vk::ImageTiling::eOptimal,
                    vk::ImageUsageFlagBits::eTransientAttachment | vk::ImageUsageFlagBits::eColorAttachment,
                    vk::MemoryPropertyFlagBits::eDeviceLocal
                );
    colorImageView = factory.createImageView(colorImage, colorFormat, vk::ImageAspectFlagBits::eColor, 1);
    //create DepthResource
    vk::Format depthFormat = factory.findSupportedFormat(
            {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint},
            vk::ImageTiling::eOptimal,
            vk::FormatFeatureFlagBits::eDepthStencilAttachment
    );
    std::tie(depthImage, depthImageMemory) = factory.createImage(Swapchain::getExtent().width, Swapchain::getExtent().height, 1, msaaSamples, depthFormat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::MemoryPropertyFlagBits::eDeviceLocal);
    depthImageView = factory.createImageView(depthImage, depthFormat, vk::ImageAspectFlagBits::eDepth, 1);
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
    createColorAndDepthResources(rct_.msaaSamples);
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
