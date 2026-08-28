#include "render/pipeline.hpp"

#include "generic/vertex.hpp"
#include "render/shader_manager.hpp"
#include "render_context.hpp"
#include <array>
#include <cstddef>
#include <stdexcept>

namespace {

// Vertex layout consumed by the raster pipeline: binding 0 = mesh vertices,
// binding 1 = per-instance model matrices. (Moved here from Vertex/
// InstanceData in Phase 4 — the data structs stay GPU-free; input layout is a
// pipeline concern.)
vk::VertexInputBindingDescription meshVertexBinding() {
    vk::VertexInputBindingDescription description;
    description.setBinding(0).setStride(sizeof(Vertex)).setInputRate(vk::VertexInputRate::eVertex);
    return description;
}

std::array<vk::VertexInputAttributeDescription, 4> meshVertexAttributes() {
    vk::VertexInputAttributeDescription posAttribute;
    posAttribute.setBinding(0).setLocation(0).setFormat(vk::Format::eR32G32B32Sfloat).setOffset(offsetof(Vertex, pos));
    vk::VertexInputAttributeDescription uvAttribute;
    uvAttribute.setBinding(0).setLocation(1).setFormat(vk::Format::eR32G32Sfloat).setOffset(offsetof(Vertex, texCoord));
    vk::VertexInputAttributeDescription norAttribute;
    norAttribute.setBinding(0).setLocation(2).setFormat(vk::Format::eR8G8B8A8Snorm).setOffset(offsetof(Vertex, normal));
    vk::VertexInputAttributeDescription tanAttribute;
    tanAttribute.setBinding(0).setLocation(3).setFormat(vk::Format::eR8G8B8A8Snorm).setOffset(offsetof(Vertex, tangent));

    return {posAttribute, uvAttribute, norAttribute, tanAttribute};
}

vk::VertexInputBindingDescription instanceBinding() {
    vk::VertexInputBindingDescription description;
    description.setBinding(1).setStride(sizeof(InstanceData)).setInputRate(vk::VertexInputRate::eInstance);
    return description;
}

std::array<vk::VertexInputAttributeDescription, 4> instanceAttributes() {
    std::array<vk::VertexInputAttributeDescription, 4> attribute;
    // mat4 model = 4 × vec4, locations 4-7
    for (uint32_t i = 0; i < 4; ++i) {
        attribute[i].setBinding(1).setLocation(4 + i).setFormat(vk::Format::eR32G32B32A32Sfloat)
                    .setOffset(sizeof(glm::vec4) * i);
    }
    return attribute;
}

}  // namespace

namespace render {

Pipeline::Pipeline(RenderContext& rct,
                   const std::vector<vk::DescriptorSetLayout>& setLayouts,
                   const GraphicsPipelineSpec& spec,
                   std::string_view spirvPath,
                   const ShaderManager& shaders)
    : rct_(rct), spirvPath_(spirvPath), shaders_(shaders), spec_(spec) {
    if (spec_.colorFormat == vk::Format::eUndefined || spec_.depthFormat == vk::Format::eUndefined) {
        throw std::invalid_argument("GraphicsPipelineSpec requires color and depth formats");
    }
    create(setLayouts, spec_);
}

void Pipeline::create(const std::vector<vk::DescriptorSetLayout>& setLayouts,
                      const GraphicsPipelineSpec& spec) {
    // Shader module — loaded/cached through the manager, never read twice.
    const auto& code  = shaders_.spirv(spirvPath_);
    auto shaderModule = shaders_.createModule(rct_.device, code);

    vk::PipelineShaderStageCreateInfo vertStageInfo{};
    vertStageInfo.setStage(vk::ShaderStageFlagBits::eVertex)
                 .setModule(*shaderModule)
                 .setPName("vertMain");
    vk::PipelineShaderStageCreateInfo fragStageInfo{};
    fragStageInfo.setStage(vk::ShaderStageFlagBits::eFragment)
                 .setModule(*shaderModule)
                 .setPName("fragMain");
    std::array<vk::PipelineShaderStageCreateInfo, 2> stages{vertStageInfo, fragStageInfo};

    // dynamic state
    std::array<vk::DynamicState, 2> dynamicStates{
        vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    vk::PipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.setDynamicStates(dynamicStates);
    vk::PipelineViewportStateCreateInfo viewportState{};
    viewportState.setViewportCount(1).setScissorCount(1);

    // vertex input (mesh vertices + per-instance data)
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

    // assembly
    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.setTopology(spec.topology);

    // rasterizer
    vk::PipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.setDepthClampEnable(vk::False)
              .setRasterizerDiscardEnable(vk::False)
              .setPolygonMode(vk::PolygonMode::eFill)
              .setCullMode(spec.cullMode)
              .setFrontFace(vk::FrontFace::eCounterClockwise)
              .setDepthBiasEnable(vk::False)
              .setLineWidth(1.0f);

    // multisampling
    vk::PipelineMultisampleStateCreateInfo multisampling{};
    multisampling.setRasterizationSamples(spec.msaaSamples)
                 .setSampleShadingEnable(vk::False);

    // depth / stencil
    vk::PipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.setDepthTestEnable(spec.depthTest ? vk::True : vk::False)
                .setDepthWriteEnable(spec.depthWrite ? vk::True : vk::False)
                .setDepthCompareOp(vk::CompareOp::eLess)
                .setDepthBoundsTestEnable(vk::False)
                .setStencilTestEnable(vk::False);

    // color blending
    vk::PipelineColorBlendAttachmentState blendAttachment{};
    blendAttachment.setBlendEnable(vk::True)
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
                 .setPAttachments(&blendAttachment);

    // push constant: one uint32 bitmask of material render flags
    vk::PushConstantRange pushConstantRange{};
    pushConstantRange.setStageFlags(vk::ShaderStageFlagBits::eFragment)
                     .setOffset(0)
                     .setSize(sizeof(uint32_t));

    vk::PipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.setSetLayouts(setLayouts).setPushConstantRanges(pushConstantRange);
    pipelineLayout_ = vk::raii::PipelineLayout(rct_.device, layoutInfo);

    // dynamic rendering
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
         .setLayout(*pipelineLayout_)
         .setRenderPass(nullptr);
    chain.get<vk::PipelineRenderingCreateInfo>()
         .setColorAttachmentCount(1)
         .setPColorAttachmentFormats(&spec.colorFormat)
         .setDepthAttachmentFormat(spec.depthFormat);

    pipeline_ = vk::raii::Pipeline(rct_.device, nullptr,
                                   chain.get<vk::GraphicsPipelineCreateInfo>());
}

}  // namespace render
