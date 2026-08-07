#pragma once

#include "render_context.hpp"
#include "vulkan/vulkan.hpp"
#include <cstdint>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

class VulkanDevice {
    public:
        struct CreateInfo{
            const char*                 appName;
            bool                        enableValidationLayers_;
            std::vector<const char*>    validationLayers_;
            std::vector<const char*>    requiredDeviceExtensions_;
            GLFWwindow*                 window_;
        };

        void init(const CreateInfo& info);

        vk::raii::Context           context_;
        vk::raii::Instance          instance       = nullptr;
        vk::raii::PhysicalDevice    physicalDevice = nullptr;
        vk::raii::Device            device         = nullptr;
        uint32_t                    graphicsQueueIndex = ~0;
        uint32_t                    transferQueueIndex = ~0;

        vk::raii::Queue             graphicsQueue  = nullptr;
        vk::raii::Queue             transferQueue  = nullptr;
        vk::SampleCountFlagBits     msaaSamples    = vk::SampleCountFlagBits::e1;

        RenderContext renderContext() {
            return { physicalDevice, device, msaaSamples };
        }

    private:
        CreateInfo                  info_;
        void createInstance();
        void pickPhysicalDevice();
        bool isDeviceSuitable(vk::raii::PhysicalDevice const &physicalDevice);
        //void checkFeatureSupport();
        void createLogicalDevice();
        void setSampleCount();

        std::vector<const char*> GetRequiredExtension();
};