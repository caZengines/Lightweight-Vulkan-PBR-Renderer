#pragma once

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

namespace render {

// Dynamic-rendering attachment handles for one frame.
// Produced by Renderer; borrowed by CommandRecorder and Pass.
struct RenderTarget {
    vk::Image     presentImage = nullptr;
    vk::ImageView presentView  = nullptr;
    vk::Image     msaaImage    = nullptr;
    vk::ImageView msaaView     = nullptr;
    vk::Image     depthImage   = nullptr;
    vk::ImageView depthView    = nullptr;

    vk::Extent2D extent{};
    vk::Format   colorFormat = vk::Format::eUndefined;
    vk::Format   depthFormat = vk::Format::eUndefined;
    vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1;

    [[nodiscard]] bool valid() const noexcept {
        return presentImage && presentView &&
               msaaImage && msaaView &&
               depthImage && depthView &&
               extent.width > 0 && extent.height > 0 &&
               colorFormat != vk::Format::eUndefined &&
               depthFormat != vk::Format::eUndefined;
    }
};

}  // namespace render
