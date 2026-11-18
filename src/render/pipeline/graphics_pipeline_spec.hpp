#pragma once

#include <cstdint>
#include <string>
#include <vector>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

namespace render {

// One vertex buffer binding of a graphics pipeline.
struct VertexBindingSpec {
    uint32_t             binding = 0;
    uint32_t             stride  = 0;
    vk::VertexInputRate  rate    = vk::VertexInputRate::eVertex;

    friend bool operator==(const VertexBindingSpec&, const VertexBindingSpec&) = default;
};

// One shader input attribute: location <- (binding, format, offset).
struct VertexAttributeSpec {
    uint32_t    location = 0;
    uint32_t    binding  = 0;
    vk::Format  format   = vk::Format::eUndefined;
    uint32_t    offset   = 0;

    friend bool operator==(const VertexAttributeSpec&, const VertexAttributeSpec&) = default;
};

// Vertex input state of one pipeline. Empty = the pipeline reads no vertex
// buffers (point/fullscreen draws that derive everything from SV_VertexID).
struct VertexLayoutSpec {
    std::vector<VertexBindingSpec>   bindings;
    std::vector<VertexAttributeSpec> attributes;

    friend bool operator==(const VertexLayoutSpec&, const VertexLayoutSpec&) = default;
};

struct SpecializationConstantValue {
    uint32_t id = 0;
    uint32_t value = 0;

    friend bool operator==(const SpecializationConstantValue&, 
                           const SpecializationConstantValue&) = default;
};

// Value-type description of one graphics pipeline: attachment formats,
// fixed-function state, vertex input and shader entry points.
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

    VertexLayoutSpec        vertexLayout;

    std::vector<SpecializationConstantValue> SpecializationConstants;

    std::string vertEntry = "vertMain";
    std::string fragEntry = "fragMain";

    friend bool operator==(const GraphicsPipelineSpec&,
                           const GraphicsPipelineSpec&) = default;
};

}  // namespace render
