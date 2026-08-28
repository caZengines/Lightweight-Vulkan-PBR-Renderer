#pragma once
#include "platform/utils.hpp"

#include <cstdint>
#include <string>
#include <vector>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan.hpp>

namespace app {

// every content-facing knob lives here —
// window setup, validation/device extensions, presentation, quality, paths.
struct Config {
    // --- Window ---
    uint32_t    width     = 1920;
    uint32_t    height    = 1080;
    std::string title     = "C' Vulkan";
    bool        resizable = true;

    // --- Device & validation ---
    bool enableValidationLayers =
#ifdef NDEBUG
        false;
#else
        true;
#endif
    std::vector<const char*> validationLayers = {"VK_LAYER_KHRONOS_validation"};
    // Swapchain is the only required device extension today; the ray-tracing
    // extensions (see refactor plan §6) will be appended here later.
    std::vector<const char*> requiredDeviceExtensions = {vk::KHRSwapchainExtensionName};

    // --- Rendering quality / presentation ---
    // msaaSamples is clamped to device-supported counts at renderer assembly.
    uint32_t           msaaSamples            = 4;
    vk::PresentModeKHR preferredPresentMode   = vk::PresentModeKHR::eMailbox;

    // Absolute asset root (project root).
    std::string assetRoot;

    // Asset paths — stored relative, resolved to absolute in the constructor.
    std::string modelPath         = "assets/models/scene.gltf";
    std::string rockPath          = "assets/models/rock.obj";
    std::string planetPath        = "assets/models/planet.obj";
    std::string texturePath       = "assets/textures/container.png";
    std::string rockTexturePath   = "assets/textures/rock.png";
    std::string marsTexturePath   = "assets/textures/mars.png";
    std::string normalTexturePath = "assets/textures/container_normal_OpenGL.png";
    std::string shaderPath        = "shaders/slang.spv";

    Config() {
        assetRoot         = platform::PlatformUtils::assetRoot();
        modelPath         = platform::PlatformUtils::assetPath(modelPath);
        rockPath          = platform::PlatformUtils::assetPath(rockPath);
        planetPath        = platform::PlatformUtils::assetPath(planetPath);
        texturePath       = platform::PlatformUtils::assetPath(texturePath);
        rockTexturePath   = platform::PlatformUtils::assetPath(rockTexturePath);
        marsTexturePath   = platform::PlatformUtils::assetPath(marsTexturePath);
        normalTexturePath = platform::PlatformUtils::assetPath(normalTexturePath);
        shaderPath        = platform::PlatformUtils::assetPath(shaderPath);
    }
};

}  // namespace app
