#pragma once
#include <memory>
#include <vector>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "render_context.hpp"
#include "descriptor_manager.hpp"
#include "command_manager.hpp"

inline const std::string MODEL_PATH = "../models/container.obj";
inline const std::string TEXTURE_PATH = "../textures/container.png";
inline const std::string NORMAL_PATH  = "../textures/container_normal_OpenGL.png";

class Context {
    public:
        vk::raii::DebugUtilsMessengerEXT         debugMessenger       = nullptr;
        vk::raii::SurfaceKHR                     surface              = nullptr;

        std::unique_ptr<DescriptorSetLayout>     descriptorSetLayout  = nullptr;
        struct Config {
            GLFWwindow*               window_                  = nullptr;
            bool                      enableValidationLayers_  = true;
            std::vector<const char*>  validationLayers_        = {};
            vk::SampleCountFlagBits   msaaSamples_             = vk::SampleCountFlagBits::e1;
        };

       explicit Context(const Config& cfg, vk::raii::PhysicalDevice& physicalDevice_, vk::raii::Device& device_, vk::raii::Instance& instance_, vk::raii::Context&& ct_);
        ~Context() = default;

        RenderContext renderContext() {
            return { physicalDevice, device, cfg_.msaaSamples_ };
        }

    private:
        vk::raii::Context                        ct;
        Config                                   cfg_;

        vk::raii::PhysicalDevice&                physicalDevice;
        vk::raii::Device&                        device;
        vk::raii::Instance&                      instance;
        
        static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(
            vk::DebugUtilsMessageSeverityFlagBitsEXT Severity,
            vk::DebugUtilsMessageTypeFlagsEXT Type,
            const vk::DebugUtilsMessengerCallbackDataEXT *pCallbackData,
            void *
        );

        void setupDebugMessenger();
        void createSurface();
        void createDescriptorSetLayout();
};