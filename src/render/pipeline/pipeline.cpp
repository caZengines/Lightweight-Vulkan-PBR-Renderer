#include "render/pipeline/pipeline.hpp"

#include "render/pipeline/pipeline_layout_library.hpp"
#include "resource/gpu/shader_library.hpp"
#include "resource/data/vertex.hpp"

#include <array>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

// Vertex layout consumed by the raster pipeline: binding 0 = mesh vertices,
// binding 1 = per-instance model matrices.
vk::VertexInputBindingDescription meshVertexBinding() {
    vk::VertexInputBindingDescription description;
    description.setBinding(0)
               .setStride(sizeof(resource::Vertex))
               .setInputRate(vk::VertexInputRate::eVertex);
    return description;
}

std::array<vk::VertexInputAttributeDescription, 4> meshVertexAttributes() {
    vk::VertexInputAttributeDescription posAttribute;
    posAttribute.setBinding(0).setLocation(0).setFormat(vk::Format::eR32G32B32Sfloat)
                .setOffset(offsetof(resource::Vertex, pos));
    vk::VertexInputAttributeDescription uvAttribute;
    uvAttribute.setBinding(0).setLocation(1).setFormat(vk::Format::eR32G32Sfloat)
               .setOffset(offsetof(resource::Vertex, texCoord));
    vk::VertexInputAttributeDescription norAttribute;
    norAttribute.setBinding(0).setLocation(2).setFormat(vk::Format::eR8G8B8A8Snorm)
                .setOffset(offsetof(resource::Vertex, normal));
    vk::VertexInputAttributeDescription tanAttribute;
    tanAttribute.setBinding(0).setLocation(3).setFormat(vk::Format::eR8G8B8A8Snorm)
                .setOffset(offsetof(resource::Vertex, tangent));

    return {posAttribute, uvAttribute, norAttribute, tanAttribute};
}

vk::VertexInputBindingDescription instanceBinding() {
    vk::VertexInputBindingDescription description;
    description.setBinding(1)
               .setStride(sizeof(resource::InstanceData))
               .setInputRate(vk::VertexInputRate::eInstance);
    return description;
}

std::array<vk::VertexInputAttributeDescription, 4> instanceAttributes() {
    std::array<vk::VertexInputAttributeDescription, 4> attribute;
    for (uint32_t i = 0; i < 4; ++i) {
        attribute[i].setBinding(1)
                    .setLocation(4 + i)
                    .setFormat(vk::Format::eR32G32B32A32Sfloat)
                    .setOffset(sizeof(glm::vec4) * i);
    }
    return attribute;
}

const char* stageLabel(vk::ShaderStageFlagBits stage) {
    switch (stage) {
        case vk::ShaderStageFlagBits::eVertex:   return "vertex";
        case vk::ShaderStageFlagBits::eFragment: return "fragment";
        case vk::ShaderStageFlagBits::eCompute:  return "compute";
        default:                                 return "other";
    }
}

void requireStage(const vk::PipelineShaderStageCreateInfo& stageInfo,
                  vk::ShaderStageFlagBits expected,
                  std::string_view entryPoint,
                  std::string_view spirvPath) {
    if (stageInfo.stage == expected) return;
    throw std::runtime_error("entry point '" + std::string(entryPoint) + "' in " +
                             std::string(spirvPath) + " is a " +
                             stageLabel(stageInfo.stage) + " stage, expected " +
                             stageLabel(expected));
}

}  // namespace

namespace render {

Pipeline::Pipeline(vk::raii::Pipeline&& pipeline,
                   vk::PipelineLayout layout,
                   vk::PipelineBindPoint bindPoint,
                   vk::ShaderStageFlags pushConstantStages)
    : pipeline_(std::move(pipeline)),
      layout_(layout),
      bindPoint_(bindPoint),
      pushConstantStages_(pushConstantStages) {}

Pipeline createGraphicsPipeline(vk::raii::Device& device,
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
    const auto fragStageInfo = shaders.stage(spirvPath, spec.fragEntry);
    requireStage(vertStageInfo, vk::ShaderStageFlagBits::eVertex, spec.vertEntry, spirvPath);
    requireStage(fragStageInfo, vk::ShaderStageFlagBits::eFragment, spec.fragEntry, spirvPath);

    const std::array<vk::PipelineShaderStageCreateInfo, 2> stages{vertStageInfo, fragStageInfo};

    vk::PipelineViewportStateCreateInfo viewportState{};
    viewportState.setViewportCount(1).setScissorCount(1);
    std::array<vk::DynamicState, 2> dynamicStates{
        vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    vk::PipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.setDynamicStates(dynamicStates);

    const auto vertexBindingDesc   = meshVertexBinding();
    const auto vertexAttribDesc    = meshVertexAttributes();
    const auto instanceBindingDesc = instanceBinding();
    const auto instanceAttribDesc  = instanceAttributes();
    std::vector<vk::VertexInputBindingDescription> bindingDescriptions{
        vertexBindingDesc, instanceBindingDesc};
    std::vector<vk::VertexInputAttributeDescription> attributeDescriptions(
        vertexAttribDesc.begin(), vertexAttribDesc.end());
    attributeDescriptions.insert(attributeDescriptions.end(),
                                 instanceAttribDesc.begin(), instanceAttribDesc.end());
    vk::PipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.setVertexBindingDescriptions(bindingDescriptions)
               .setVertexAttributeDescriptions(attributeDescriptions);

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
    return Pipeline(std::move(pipeline),
                    layout.getHandle(),
                    vk::PipelineBindPoint::eGraphics,
                    layout.pushConstantStages());
}

Pipeline createComputePipeline(vk::raii::Device& device,
                               const PipelineLayout& layout,
                               const resource::ShaderLibrary& shaders,
                               std::string_view spirvPath,
                               const ComputePipelineSpec& spec) {
    if (spec.entryPoint.empty()) {
        throw std::invalid_argument("ComputePipelineSpec requires an entry point");
    }

    const auto stageInfo = shaders.stage(spirvPath, spec.entryPoint);
    requireStage(stageInfo, vk::ShaderStageFlagBits::eCompute, spec.entryPoint, spirvPath);

    vk::ComputePipelineCreateInfo createInfo{};
    createInfo.setStage(stageInfo).setLayout(layout.getHandle());

    vk::raii::Pipeline pipeline(device, nullptr, createInfo);
    return Pipeline(std::move(pipeline),
                    layout.getHandle(),
                    vk::PipelineBindPoint::eCompute,
                    layout.pushConstantStages());
}

}  // namespace render
