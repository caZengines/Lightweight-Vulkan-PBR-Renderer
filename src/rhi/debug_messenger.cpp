#include "rhi/debug_messenger.hpp"

#include "platform/log.hpp"

namespace rhi {

DebugMessenger::DebugMessenger(const vk::raii::Instance& instance, bool enableValidationLayers) {
    if (!enableValidationLayers) return;

    vk::DebugUtilsMessengerCreateInfoEXT createInfo{};
    createInfo.setMessageSeverity(vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                                  vk::DebugUtilsMessageSeverityFlagBitsEXT::eError)
               .setMessageType(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                               vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                               vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance)
               .setPfnUserCallback(&callback);
    try {
        messenger_ = instance.createDebugUtilsMessengerEXT(createInfo);
    } catch (const vk::SystemError&) {
        platform::LogLocator::get().write(platform::LogLevel::Warning,
            "Debug messenger not available. Validation layers may not be enabled.");
    }
}

VKAPI_ATTR vk::Bool32 VKAPI_CALL DebugMessenger::callback(
    vk::DebugUtilsMessageSeverityFlagBitsEXT severity,
    vk::DebugUtilsMessageTypeFlagsEXT type,
    const vk::DebugUtilsMessengerCallbackDataEXT* callbackData,
    void* /*userData*/) {
    if (severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eError ||
        severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning) {
        platform::LogLocator::get().write(platform::LogLevel::Error,
            std::string("validation layer: type ") + vk::to_string(type) +
            " msg: " + callbackData->pMessage);
    }
    return VK_FALSE;
}

}  // namespace rhi
