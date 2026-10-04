#pragma once

#include <string_view>

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

    // Entry-point names are the C++↔shader interface contract.  They are spelled
    // once here rather than inside Pipeline, so a second pipeline family can name
    // its own; ShaderLibrary::stage() throws with the available names if one
    // drifts (see docs/pipeline-descriptor-refactor-plan.md D6/D7).
    std::string_view        vertEntry     = "vertMain";
    std::string_view        fragEntry     = "fragMain";

    friend constexpr bool operator==(const GraphicsPipelineSpec&,
                                     const GraphicsPipelineSpec&) = default;
};

}  // namespace render
