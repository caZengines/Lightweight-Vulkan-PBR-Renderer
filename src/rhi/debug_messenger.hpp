#pragma once

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

namespace rhi {

// Validation-layer output hook, split out of the old god-config Context.
// Created after instance creation; silently no-ops (with a log warning) when
// validation layers are disabled or the debug-utils extension is absent.
class DebugMessenger final {
public:
    // enableValidationLayers mirrors the app's global toggle; when false the
    // messenger is not created at all.
    explicit DebugMessenger(const vk::raii::Instance& instance, bool enableValidationLayers);

private:
    static VKAPI_ATTR vk::Bool32 VKAPI_CALL callback(
        vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
        vk::DebugUtilsMessageTypeFlagsEXT type,
        const vk::DebugUtilsMessengerCallbackDataEXT* callbackData,
        void* userData);

    vk::raii::DebugUtilsMessengerEXT messenger_ = nullptr;
};

}  // namespace rhi
