#pragma once

#include <string>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

namespace render {

// Value-type description of one graphics pipeline: attachment formats,
// fixed-function state, and shader entry points.
struct GraphicsPipelineSpec {
    vk::Format              colorFormat    = vk::Format::eUndefined;
    vk::Format              depthFormat    = vk::Format::eUndefined;
    vk::SampleCountFlagBits msaaSamples    = vk::SampleCountFlagBits::e1;
    vk::PrimitiveTopology   topology       = vk::PrimitiveTopology::eTriangleList;
    vk::CullModeFlags       cullMode       = vk::CullModeFlagBits::eBack;
    vk::FrontFace           frontFace      = vk::FrontFace::eCounterClockwise;
    vk::CompareOp           depthCompareOp = vk::CompareOp::eLess;
    bool                    depthTest      = true;
    bool                    depthWrite     = true;
    bool                    blendEnable    = true;

    std::string vertEntry = "vertMain";
    std::string fragEntry = "fragMain";

    friend bool operator==(const GraphicsPipelineSpec&,
                           const GraphicsPipelineSpec&) = default;
};

// Value-type description of one compute pipeline.
struct ComputePipelineSpec {
    std::string entryPoint = "csMain";

    friend bool operator==(const ComputePipelineSpec&,
                           const ComputePipelineSpec&) = default;
};

}  // namespace render
