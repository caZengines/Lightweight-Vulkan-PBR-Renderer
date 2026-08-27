#pragma once

#include <cstdint>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

namespace render {

// Value-type description of one graphics pipeline. What used to be hardcoded
// inside Pipeline::createGraphicsPipeline is now data the upper layers can
// author and vary (e.g., swap color formats or MSAA without touching code).
// Caches key on this struct, so defaulted equality must stay exhaustive.
struct GraphicsPipelineSpec {
    vk::Format              colorFormat   = vk::Format::eUndefined;
    vk::Format              depthFormat   = vk::Format::eUndefined;
    vk::SampleCountFlagBits msaaSamples   = vk::SampleCountFlagBits::e1;
    vk::PrimitiveTopology   topology      = vk::PrimitiveTopology::eTriangleList;
    vk::CullModeFlags       cullMode      = vk::CullModeFlagBits::eBack;
    bool                    depthTest     = true;
    bool                    depthWrite    = true;

    friend constexpr bool operator==(const GraphicsPipelineSpec&,
                                     const GraphicsPipelineSpec&) = default;
};

}  // namespace render
