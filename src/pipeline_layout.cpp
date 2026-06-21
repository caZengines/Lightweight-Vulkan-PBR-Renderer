#include "pipeline_layout.hpp"
#include "generic/vertex.hpp"

#include <stdexcept>
#include <fstream>
#include <array>

Pipeline::Pipeline(RenderContext& rct, vk::raii::DescriptorSetLayout& dsl, vk::Format colorFormat)
    : rct_(rct)
{
    createGraphicsPipeline(dsl, colorFormat);
}

void Pipeline::createGraphicsPipeline(vk::raii::DescriptorSetLayout& dsl, vk::Format colorFormat) {
    // shader modules
    auto code = readFile("../shaders/slang.spv");
    vk::raii::ShaderModule shaderModule = createShaderModule(code);

    vk::PipelineShaderStageCreateInfo vertStageInfo{};
    vertStageInfo.setStage(vk::ShaderStageFlagBits::eVertex)
             .setModule(*shaderModule)
             .setPName("vertMain");
    vk::PipelineShaderStageCreateInfo fragStageInfo{};
    fragStageInfo.setStage(vk::ShaderStageFlagBits::eFragment)
             .setModule(*shaderModule)
             .setPName("fragMain");
    vk::PipelineShaderStageCreateInfo stages[] = {vertStageInfo, fragStageInfo};

    // dynamic state
    std::array<vk::DynamicState, 2> dynamicStates = {
        vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    vk::PipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.setDynamicStates(dynamicStates);
    vk::PipelineViewportStateCreateInfo viewportState;
    viewportState.setViewportCount(1).setScissorCount(1);

    // vertex input
    auto bindingDesc    = Vertex::getBindingDescription();
    auto attribDescs    = Vertex::getAttributeDescription();
    vk::PipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.setVertexBindingDescriptions(bindingDesc)
               .setVertexAttributeDescriptions(attribDescs);

    // assembly
    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.setTopology(vk::PrimitiveTopology::eTriangleList);

    // rasterizer
    vk::PipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.setDepthClampEnable(vk::False)
              .setRasterizerDiscardEnable(vk::False)
              .setPolygonMode(vk::PolygonMode::eFill)
              .setCullMode(vk::CullModeFlagBits::eBack)
              .setFrontFace(vk::FrontFace::eCounterClockwise)
              .setDepthBiasEnable(vk::False)
              .setLineWidth(1.0f);

    // multisampling
    vk::PipelineMultisampleStateCreateInfo multisampling{};
    multisampling.setRasterizationSamples(rct_.msaaSamples)
                 .setSampleShadingEnable(vk::False);

    // depth / stencil
    vk::PipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.setDepthTestEnable(vk::True)
                .setDepthWriteEnable(vk::True)
                .setDepthCompareOp(vk::CompareOp::eLess)
                .setDepthBoundsTestEnable(vk::False)
                .setStencilTestEnable(vk::False);

    // color blending
    vk::PipelineColorBlendAttachmentState blendAttachment;
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

    // pipeline layout
    vk::PipelineLayoutCreateInfo layoutInfo;
    layoutInfo.setSetLayouts(*dsl).setPushConstantRangeCount(0);
    pipelineLayout = vk::raii::PipelineLayout(rct_.device, layoutInfo);

    // dynamic rendering
    vk::StructureChain<vk::GraphicsPipelineCreateInfo,
                        vk::PipelineRenderingCreateInfo> pipelineCreateInfoChain;
    pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>()
         .setStages(stages)
         .setPVertexInputState(&vertexInput)
         .setPInputAssemblyState(&inputAssembly)
         .setPDepthStencilState(&depthStencil)
         .setPViewportState(&viewportState)
         .setPRasterizationState(&rasterizer)
         .setPMultisampleState(&multisampling)
         .setPColorBlendState(&colorBlending)
         .setPDynamicState(&dynamicState)
         .setLayout(*pipelineLayout)
         .setRenderPass(nullptr);

    pipelineCreateInfoChain.get<vk::PipelineRenderingCreateInfo>()
         .setColorAttachmentCount(1)
         .setPColorAttachmentFormats(&colorFormat)
         .setDepthAttachmentFormat(findDepthFormat());

    pipeline = vk::raii::Pipeline(rct_.device, nullptr,
                                           pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>());
}

std::vector<char> Pipeline::readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open())
        throw std::runtime_error("failed to open file: " + filename);

    size_t size = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(size);
    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(size));
    file.close();
    return buffer;
}

vk::raii::ShaderModule Pipeline::createShaderModule(const std::vector<char>& code) const {
    vk::ShaderModuleCreateInfo ci{};
    ci.setCodeSize(code.size())
      .setPCode(reinterpret_cast<const uint32_t*>(code.data()));
    return vk::raii::ShaderModule(rct_.device, ci);
}

vk::Format Pipeline::findDepthFormat() const {
    return findSupportedFormat(
        {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint,
         vk::Format::eD24UnormS8Uint},
        vk::ImageTiling::eOptimal,
        vk::FormatFeatureFlagBits::eDepthStencilAttachment);
}

vk::Format Pipeline::findSupportedFormat(const std::vector<vk::Format>& candidates,
                                          vk::ImageTiling tiling,
                                          vk::FormatFeatureFlags features) const {
    for (auto format : candidates) {
        vk::FormatProperties props = rct_.physicalDevice.getFormatProperties(format);
        if (tiling == vk::ImageTiling::eLinear &&
            (props.linearTilingFeatures & features) == features)
            return format;
        if (tiling == vk::ImageTiling::eOptimal &&
            (props.optimalTilingFeatures & features) == features)
            return format;
    }
    throw std::runtime_error("failed to find supported format!");
}
