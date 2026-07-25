#include "c_engine.hpp" 
#include "vulkan/vulkan.hpp"
#include "context.hpp"
#include <iostream>

Context::Context(const Config& cfg, vk::raii::PhysicalDevice& physicalDevice, vk::raii::Device& device, vk::raii::Instance& instance) 
    : cfg_(cfg), physicalDevice_(physicalDevice), device_(device), instance_(instance)
{
    setupDebugMessenger();
    createSurface();
}

void Context::setupDebugMessenger(){
    if(!cfg_.enableValidationLayers_) return;

    vk::DebugUtilsMessengerCreateInfoEXT createInfo{};
    createInfo.setMessageSeverity(vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                                      vk::DebugUtilsMessageSeverityFlagBitsEXT::eError)
               .setMessageType(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                                  vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                                  vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance)
                .setPfnUserCallback(&debugCallback);

    debugMessenger = instance_.createDebugUtilsMessengerEXT( createInfo );
}
VKAPI_ATTR vk::Bool32 VKAPI_CALL Context::debugCallback(
    vk::DebugUtilsMessageSeverityFlagBitsEXT Severity,
    vk::DebugUtilsMessageTypeFlagsEXT Type,
    const vk::DebugUtilsMessengerCallbackDataEXT *pCallbackData,
    void *
    ){
    if (Severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eError || Severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning)
    {
        std::cerr << "validation layer: type " << to_string(Type) << " msg: " << pCallbackData->pMessage << std::endl;
    }
    return VK_FALSE;
}

void Context::createSurface(){
    VkSurfaceKHR surface_;
    if (glfwCreateWindowSurface(*instance_, cfg_.window_, nullptr, &surface_) != 0)
    {
        throw std::runtime_error("failed to create window surface!");
    }
    surface = vk::raii::SurfaceKHR(instance_, surface_);
}