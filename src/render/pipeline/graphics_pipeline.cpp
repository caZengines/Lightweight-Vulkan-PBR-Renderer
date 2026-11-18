#include "render/pipeline/graphics_pipeline.hpp"

#include "render/pipeline/graphics_pipeline_spec.hpp"
#include "render/pipeline/pipeline_layout_library.hpp"
#include "render/pipeline/shader_stage.hpp"
#include "resource/gpu/shader_library.hpp"
#include "resource/data/vertex.hpp"
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include "vulkan/vulkan.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

// Turns the value-type vertex layout into the vk descriptions, rejecting
// attributes that point at a binding the layout never declares.
vk::PipelineVertexInputStateCreateInfo buildVertexInput(const render::VertexLayoutSpec& layout,
                                                        std::vector<vk::VertexInputBindingDescription>& bindings,
                                                        std::vector<vk::VertexInputAttributeDescription>& attributes) {
    bindings.reserve(layout.bindings.size());
    for (const render::VertexBindingSpec& binding : layout.bindings) {
        vk::VertexInputBindingDescription description;
        description.setBinding(binding.binding)
                   .setStride(binding.stride)
                   .setInputRate(binding.rate);
        bindings.push_back(description);
    }

    attributes.reserve(layout.attributes.size());
    for (const render::VertexAttributeSpec& attribute : layout.attributes) {
        const bool known = std::ranges::any_of(
            layout.bindings,
            [&](const render::VertexBindingSpec& binding) {
                return binding.binding == attribute.binding;
            });
        if (!known) {
            throw std::invalid_argument("vertex attribute location " +
                                        std::to_string(attribute.location) +
                                        " references undeclared binding " +
                                        std::to_string(attribute.binding));
        }
        vk::VertexInputAttributeDescription description;
        description.setLocation(attribute.location)
                   .setBinding(attribute.binding)
                   .setFormat(attribute.format)
                   .setOffset(attribute.offset);
        attributes.push_back(description);
    }

    vk::PipelineVertexInputStateCreateInfo vertexInput;
    vertexInput.setVertexBindingDescriptions(bindings)
               .setVertexAttributeDescriptions(attributes);
    return vertexInput;
}

}  // namespace

namespace render {

GraphicsPipeline::GraphicsPipeline(vk::raii::Pipeline&& pipeline,
                                   vk::PipelineLayout layout,
                                   vk::ShaderStageFlags pushConstantStages)
    : PipelineBase(std::move(pipeline), layout,
                   vk::PipelineBindPoint::eGraphics, pushConstantStages) {}

VertexLayoutSpec meshVertexLayout() {
    VertexLayoutSpec layout;
    layout.bindings = {
        VertexBindingSpec{0, static_cast<uint32_t>(sizeof(resource::Vertex)),
                          vk::VertexInputRate::eVertex},
        VertexBindingSpec{1, static_cast<uint32_t>(sizeof(resource::InstanceData)),
                          vk::VertexInputRate::eInstance},
    };
    layout.attributes = {
        VertexAttributeSpec{0, 0, vk::Format::eR32G32B32Sfloat,
                            static_cast<uint32_t>(offsetof(resource::Vertex, pos))},
        VertexAttributeSpec{1, 0, vk::Format::eR32G32Sfloat,
                            static_cast<uint32_t>(offsetof(resource::Vertex, texCoord0))},
        VertexAttributeSpec{2, 0, vk::Format::eR32G32Sfloat,
                            static_cast<uint32_t>(offsetof(resource::Vertex, texCoord1))},
        VertexAttributeSpec{3, 0, vk::Format::eR8G8B8A8Snorm,
                            static_cast<uint32_t>(offsetof(resource::Vertex, normal))},
        VertexAttributeSpec{4, 0, vk::Format::eR8G8B8A8Snorm,
                            static_cast<uint32_t>(offsetof(resource::Vertex, tangent))},
    };
    for (uint32_t i = 0; i < 4; ++i) {
        layout.attributes.emplace_back(
            VertexAttributeSpec{5 + i, 1, vk::Format::eR32G32B32A32Sfloat,
                                static_cast<uint32_t>(sizeof(glm::vec4) * i)});
    }
    return layout;
}

GraphicsPipeline createGraphicsPipeline(vk::raii::Device& device,
                                       const PipelineLayout& layout,
                                       const resource::ShaderLibrary& shaders,
                                       std::string_view spirvPath,
                                       const GraphicsPipelineSpec& spec) {
    if (spec.colorFormat == vk::Format::eUndefined ||
        spec.depthFormat == vk::Format::eUndefined) {
        throw std::invalid_argument("GraphicsPipelineSpec requires color and depth formats");
    }
    if (spec.vertEntry.empty() || spec.fragEntry.empty()) {
        throw std::invalid_argument("GraphicsPipelineSpec requires vertex and fragment entry points");
    }

    const auto vertStageInfo = shaders.stage(spirvPath, spec.vertEntry);
    auto fragStageInfo = shaders.stage(spirvPath, spec.fragEntry);
    detail::requireStage(vertStageInfo, vk::ShaderStageFlagBits::eVertex, spec.vertEntry, spirvPath);
    detail::requireStage(fragStageInfo, vk::ShaderStageFlagBits::eFragment, spec.fragEntry, spirvPath);

    // specialization constants
    std::vector<vk::SpecializationMapEntry> specEntries;
    std::vector<uint32_t>                   specValues;
    vk::SpecializationInfo                  fragSpecInfo{};
    if(!spec.SpecializationConstants.empty()) {
        specEntries.reserve(spec.SpecializationConstants.size());
        specValues.reserve(spec.SpecializationConstants.size());
        for (uint32_t i = 0; i < spec.SpecializationConstants.size(); ++i) {
            const auto& c = spec.SpecializationConstants[i];
            vk::SpecializationMapEntry e;
            e.setConstantID(c.id)
             .setOffset(i * sizeof(uint32_t))
             .setSize(sizeof(uint32_t));
            specEntries.emplace_back(e);
            specValues.emplace_back(c.value);
        }
        fragSpecInfo.setMapEntries(specEntries)
                    .setDataSize(specValues.size() * sizeof(uint32_t))
                    .setPData(specValues.data());
        fragStageInfo.setPSpecializationInfo(&fragSpecInfo);
    }

    const std::array<vk::PipelineShaderStageCreateInfo, 2> stages{vertStageInfo, fragStageInfo};

    // dynamic state
    vk::PipelineViewportStateCreateInfo viewportState{};
    viewportState.setViewportCount(1).setScissorCount(1);
    std::array<vk::DynamicState, 2> dynamicStates{
        vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    vk::PipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.setDynamicStates(dynamicStates);

    std::vector<vk::VertexInputBindingDescription>   bindingDescriptions;
    std::vector<vk::VertexInputAttributeDescription> attributeDescriptions;
    const vk::PipelineVertexInputStateCreateInfo vertexInput =
        buildVertexInput(spec.vertexLayout, bindingDescriptions, attributeDescriptions);

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.setTopology(spec.topology)
                 .setPrimitiveRestartEnable(vk::False);
    
    vk::PipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.setDepthClampEnable(vk::False)
              .setRasterizerDiscardEnable(vk::False)
              .setPolygonMode(vk::PolygonMode::eFill)
              .setCullMode(spec.cullMode)
              .setFrontFace(spec.frontFace)
              .setDepthBiasEnable(vk::False)
              .setLineWidth(1.0f);

    vk::PipelineMultisampleStateCreateInfo multisampling{};
    multisampling.setRasterizationSamples(spec.msaaSamples)
                 .setSampleShadingEnable(vk::False);

    vk::PipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.setDepthTestEnable(spec.depthTest ? vk::True : vk::False)
                .setDepthWriteEnable(spec.depthWrite ? vk::True : vk::False)
                .setDepthCompareOp(spec.depthCompareOp)
                .setDepthBoundsTestEnable(vk::False)
                .setStencilTestEnable(vk::False);

    vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.setBlendEnable(spec.blendEnable ? vk::True : vk::False)
                       .setSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha)
                       .setDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
                       .setColorBlendOp(vk::BlendOp::eAdd)
                       .setSrcAlphaBlendFactor(vk::BlendFactor::eOne)
                       .setDstAlphaBlendFactor(vk::BlendFactor::eZero)
                       .setAlphaBlendOp(vk::BlendOp::eAdd)
                       .setColorWriteMask(vk::ColorComponentFlagBits::eR |
                                          vk::ColorComponentFlagBits::eG |
                                          vk::ColorComponentFlagBits::eB |
                                          vk::ColorComponentFlagBits::eA);
    vk::PipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.setLogicOpEnable(vk::False)
                 .setLogicOp(vk::LogicOp::eCopy)
                 .setAttachmentCount(1)
                 .setPAttachments(&colorBlendAttachment);

    vk::StructureChain<vk::GraphicsPipelineCreateInfo,
                       vk::PipelineRenderingCreateInfo> chain{};
    chain.get<vk::GraphicsPipelineCreateInfo>()
         .setStages(stages)
         .setPVertexInputState(&vertexInput)
         .setPInputAssemblyState(&inputAssembly)
         .setPDepthStencilState(&depthStencil)
         .setPViewportState(&viewportState)
         .setPRasterizationState(&rasterizer)
         .setPMultisampleState(&multisampling)
         .setPColorBlendState(&colorBlending)
         .setPDynamicState(&dynamicState)
         .setLayout(layout.getHandle())
         .setRenderPass(nullptr);
    chain.get<vk::PipelineRenderingCreateInfo>()
         .setColorAttachmentCount(1)
         .setPColorAttachmentFormats(&spec.colorFormat)
         .setDepthAttachmentFormat(spec.depthFormat);

    vk::raii::Pipeline pipeline(device, nullptr,
                                chain.get<vk::GraphicsPipelineCreateInfo>());
    return GraphicsPipeline(std::move(pipeline),
                            layout.getHandle(),
                            layout.pushConstantStages());
}

}  // namespace render
