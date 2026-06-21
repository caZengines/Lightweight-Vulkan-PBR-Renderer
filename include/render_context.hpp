#pragma once

#define VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>


struct RenderContext {
    vk::raii::PhysicalDevice& physicalDevice;
    vk::raii::Device&         device;
    vk::SampleCountFlagBits  msaaSamples;
};
