#pragma once

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

namespace render {

// User-tunable rendering knobs injected from app::Config via the composition
// root. Replaces the former hardcoded MSAA 4x and mailbox-only present pick
// (known debt items #8 / §1.4-11 of the refactor plan).
struct RenderSettings {
    vk::SampleCountFlagBits msaaSamples          = vk::SampleCountFlagBits::e4;
    vk::PresentModeKHR      preferredPresentMode = vk::PresentModeKHR::eMailbox;  // falls back to FIFO
};

}  // namespace render
