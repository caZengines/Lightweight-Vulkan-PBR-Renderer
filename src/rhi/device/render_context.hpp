#pragma once


#include <vulkan/vulkan_raii.hpp>

// note render_context.hpp keeps a COPY-by-value of msaaSamples — see
// Renderer::Dependencies for how settings now flow.

struct RenderContext {
    vk::raii::PhysicalDevice& physicalDevice;
    vk::raii::Device&         device;
    vk::SampleCountFlagBits  msaaSamples;
};
