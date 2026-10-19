#pragma once

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

namespace render {

// User-tunable rendering knobs injected from app::Config via the composition
// root.
struct RenderSettings {
    vk::SampleCountFlagBits msaaSamples          = vk::SampleCountFlagBits::e4;
    vk::PresentModeKHR      preferredPresentMode = vk::PresentModeKHR::eMailbox;  // falls back to FIFO
    // Frame-pacing target for VK_EXT_present_timing. 0 disables the request.
    double                  targetFPS            = 120.0;
};

}  // namespace render
