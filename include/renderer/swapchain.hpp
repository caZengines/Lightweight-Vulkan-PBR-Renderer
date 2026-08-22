#pragma once
#include <vector>
#include "platform/window.hpp"
#include "render_context.hpp"

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>
#include "vma_allocator.hpp"



class Swapchain final {
public:
    explicit Swapchain(RenderContext& rct, VmaAllocator alloc, vk::raii::SurfaceKHR& surface, platform::Window& window);
    void recreateSwapChain(vk::raii::SurfaceKHR& surface, platform::Window& window);
    void cleanupSwapChain();

    ~Swapchain() = default;

    struct Image {
        std::vector<vk::Image>               images;
        std::vector<vk::raii::ImageView>     ImageViews;
    } Image_;

    vk::Extent2D         getExtent()       const { return extent; }
    vk::SurfaceFormatKHR getSurfaceFormat()       const { return surfaceformat; }
    vk::raii::SwapchainKHR& swapChain()          { return swapChain_; }

    const VkImage& getcolorImage() const { return colorImage_.getHandle(); }
    const VkImage& getDepthImage() const { return depthImage_.getHandle(); }

    const vk::raii::ImageView& getColorImageView() const { return colorImageView; }
    const vk::raii::ImageView& getDepthImageView() const { return depthImageView; }

private:
    RenderContext&                       rct_;
    vk::raii::SwapchainKHR               swapChain_       = nullptr;
    vk::SurfaceFormatKHR                 surfaceformat;
    vk::Extent2D                         extent;
    VmaAllocator                         allocator_       = nullptr;

    VmaImage                             colorImage_;
    vk::raii::ImageView                  colorImageView   = nullptr;
    VmaImage                             depthImage_;
    vk::raii::ImageView                  depthImageView   = nullptr;

    static vk::Extent2D         chooseExtent(vk::SurfaceCapabilitiesKHR const&, platform::Window&);
    static uint32_t             chooseMinImageCount(vk::SurfaceCapabilitiesKHR const&);
    static vk::SurfaceFormatKHR chooseFormat(const std::vector<vk::SurfaceFormatKHR>&);
    static vk::PresentModeKHR   choosePresentMode(std::vector<vk::PresentModeKHR> const&);

    void createSwapChain(vk::raii::SurfaceKHR& surface, platform::Window& window);
    void createImageViews();
    void createColorAndDepthResources(VmaAllocator alloc, vk::SampleCountFlagBits msaaSamples);
};
