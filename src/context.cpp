#include "context.hpp"
#include "platform/log.hpp"

Context::Context(const Config& cfg, vk::raii::PhysicalDevice& physicalDevice, vk::raii::Device& device, vk::raii::Instance& instance, platform::Window& window)
    : cfg_(cfg), physicalDevice_(physicalDevice), device_(device), instance_(instance), window_(window)
{
    setupDebugMessenger();
    createSurface();
}

void Context::setupDebugMessenger() {
    if(!cfg_.enableValidationLayers_) return;

    vk::DebugUtilsMessengerCreateInfoEXT createInfo{};
    createInfo.setMessageSeverity(vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                                      vk::DebugUtilsMessageSeverityFlagBitsEXT::eError)
               .setMessageType(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                                  vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                                  vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance)
                .setPfnUserCallback(&debugCallback);
    
    try {
        debugMessenger = instance_.createDebugUtilsMessengerEXT( createInfo );
    } catch(vk::SystemError &err) {
        platform::LogLocator::get().write(platform::LogLevel::Warning,
            "Debug messenger not available. Validation layers may not be enabled.");
    }
}
VKAPI_ATTR vk::Bool32 VKAPI_CALL Context::debugCallback(
    vk::DebugUtilsMessageSeverityFlagBitsEXT Severity,
    vk::DebugUtilsMessageTypeFlagsEXT Type,
    const vk::DebugUtilsMessengerCallbackDataEXT *pCallbackData,
    void *
    ){
    if (Severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eError || Severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning)
    {
        platform::LogLocator::get().write(platform::LogLevel::Error,
            std::string("validation layer: type ") + vk::to_string(Type) + " msg: " + pCallbackData->pMessage);
    }
    return VK_FALSE;
}

void Context::createSurface() {
    surface = vk::raii::SurfaceKHR(instance_, window_.createSurface(*instance_));
}
