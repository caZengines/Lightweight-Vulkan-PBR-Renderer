#pragma once
#include <string>
#include <vector>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include "platform/window.hpp"
#include "render_context.hpp"

// Asset paths moved to app::Config (Phase 2) — no "../" relative paths here.

class Context {
    public:
        vk::raii::DebugUtilsMessengerEXT         debugMessenger       = nullptr;
        vk::raii::SurfaceKHR                     surface              = nullptr;

        struct Config {
            bool                      enableValidationLayers_  = true;
            std::vector<const char*>  validationLayers_        = {};
            vk::SampleCountFlagBits   msaaSamples_             = vk::SampleCountFlagBits::e1;
        };

       explicit Context(const Config& cfg, vk::raii::PhysicalDevice& physicalDevice, vk::raii::Device& device, vk::raii::Instance& instance, platform::Window& window);
        ~Context() = default;

    private:
        Config                                   cfg_;

        vk::raii::PhysicalDevice&                physicalDevice_;
        vk::raii::Device&                        device_;
        vk::raii::Instance&                      instance_;
        platform::Window&                        window_;
        
        static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(
            vk::DebugUtilsMessageSeverityFlagBitsEXT Severity,
            vk::DebugUtilsMessageTypeFlagsEXT Type,
            const vk::DebugUtilsMessengerCallbackDataEXT *pCallbackData,
            void *
        );

        void setupDebugMessenger();
        void createSurface();
};
