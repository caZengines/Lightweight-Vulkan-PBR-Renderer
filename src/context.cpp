#include "c_engine.hpp" 
#include "vulkan/vulkan.hpp"
#include "context.hpp"
#include <iostream>

Context::Context(const Config& cfg, vk::raii::PhysicalDevice& physicalDevice_, vk::raii::Device& device_, vk::raii::Instance& instance_, vk::raii::Context&& ct_) 
    : cfg_(cfg), physicalDevice(physicalDevice_), device(device_), instance(instance_), ct(std::move(ct_))
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

    debugMessenger = instance.createDebugUtilsMessengerEXT( createInfo );
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
    VkSurfaceKHR _surface;
    if (glfwCreateWindowSurface(*instance, cfg_.window_, nullptr, &_surface) != 0)
    {
        throw std::runtime_error("failed to create window surface!");
    }
    surface = vk::raii::SurfaceKHR(instance, _surface);
}