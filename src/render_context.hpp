#pragma once

#define VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

// Frames-in-flight constant moved to render/frame_resources.hpp (Phase 3);
// note render_context.hpp keeps a COPY-by-value of msaaSamples — see
// Renderer::Dependencies for how settings now flow.

struct RenderContext {
    vk::raii::PhysicalDevice& physicalDevice;
    vk::raii::Device&         device;
    vk::SampleCountFlagBits  msaaSamples;
};
