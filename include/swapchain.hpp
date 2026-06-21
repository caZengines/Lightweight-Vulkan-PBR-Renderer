#pragma once
#include <vector>
#include "render_context.hpp"

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>


class Swapchain {
public:
    explicit Swapchain(RenderContext& rct, vk::raii::SurfaceKHR& surface, GLFWwindow* window);
    void recreateSwapChain(vk::raii::SurfaceKHR& surface, GLFWwindow* window);
    void cleanupSwapChain();

    ~Swapchain() = default;

    struct Image {
        std::vector<vk::Image>               images;
        std::vector<vk::raii::ImageView>     ImageViews;
    } Image_;

    vk::Extent2D         getExtent()       const { return extent; }
    vk::SurfaceFormatKHR getSurfaceFormat()       const { return surfaceformat; }
    vk::raii::SwapchainKHR& swapChain()          { return swapChain_; }

    vk::raii::Image& getcolorImage() { return colorImage; }
    vk::raii::Image& getDepthImage() { return depthImage; }

    const vk::raii::ImageView& getColorImageView() const { return colorImageView; }
    const vk::raii::ImageView& getDepthImageView() const { return depthImageView; }

private:
    RenderContext&                       rct_;
    vk::raii::SwapchainKHR               swapChain_       = nullptr;
    vk::SurfaceFormatKHR                 surfaceformat;
    vk::Extent2D                         extent;

    vk::raii::DeviceMemory               colorImageMemory = nullptr;
    vk::raii::Image                      colorImage       = nullptr;
    vk::raii::ImageView                  colorImageView   = nullptr;

    vk::raii::DeviceMemory               depthImageMemory = nullptr;
    vk::raii::Image                      depthImage       = nullptr;
    vk::raii::ImageView                  depthImageView   = nullptr;

    static vk::Extent2D         chooseExtent(vk::SurfaceCapabilitiesKHR const&, GLFWwindow*);
    static uint32_t             chooseMinImageCount(vk::SurfaceCapabilitiesKHR const&);
    static vk::SurfaceFormatKHR chooseFormat(const std::vector<vk::SurfaceFormatKHR>&);
    static vk::PresentModeKHR   choosePresentMode(std::vector<vk::PresentModeKHR> const&);

    void createSwapChain(vk::raii::SurfaceKHR& surface, GLFWwindow* window);
    void createImageViews();
    void createColorAndDepthResources(vk::SampleCountFlagBits msaaSamples);
};
