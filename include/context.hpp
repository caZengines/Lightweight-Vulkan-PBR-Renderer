#pragma once
#include <vector>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "render_context.hpp"

inline const std::string MODEL_PATH = "../models/container.obj";
inline const std::string ROCK_PATH  = "../models/rock.obj";
inline const std::string PLANET_PATH = "../models/planet.obj";
inline const std::string TEXTURE_PATH = "../textures/container.png";
inline const std::string ROCK_TEXTURE_PATH  = "../textures/rock.png";
inline const std::string MARS_PATH          = "../textures/mars.png";
inline const std::string NORMAL_PATH  = "../textures/container_normal_OpenGL.png";

class Context {
    public:
        vk::raii::DebugUtilsMessengerEXT         debugMessenger       = nullptr;
        vk::raii::SurfaceKHR                     surface              = nullptr;

        struct Config {
            GLFWwindow*               window_                  = nullptr;
            bool                      enableValidationLayers_  = true;
            std::vector<const char*>  validationLayers_        = {};
            vk::SampleCountFlagBits   msaaSamples_             = vk::SampleCountFlagBits::e1;
        };

       explicit Context(const Config& cfg, vk::raii::PhysicalDevice& physicalDevice, vk::raii::Device& device, vk::raii::Instance& instance);
        ~Context() = default;

    private:
        Config                                   cfg_;

        vk::raii::PhysicalDevice&                physicalDevice_;
        vk::raii::Device&                        device_;
        vk::raii::Instance&                      instance_;
        
        static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(
            vk::DebugUtilsMessageSeverityFlagBitsEXT Severity,
            vk::DebugUtilsMessageTypeFlagsEXT Type,
            const vk::DebugUtilsMessengerCallbackDataEXT *pCallbackData,
            void *
        );

        void setupDebugMessenger();
        void createSurface();
};