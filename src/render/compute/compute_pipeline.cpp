#include "render/compute/compute_pipeline.hpp"

#include "render/pipeline/pipeline_layout_library.hpp"
#include "render/pipeline/shader_stage.hpp"
#include "resource/gpu/shader_library.hpp"

#include <stdexcept>
#include <utility>

namespace render {

ComputePipeline::ComputePipeline(vk::raii::Pipeline&& pipeline,
                                 vk::PipelineLayout layout,
                                 vk::ShaderStageFlags pushConstantStages)
    : PipelineBase(std::move(pipeline), layout,
                   vk::PipelineBindPoint::eCompute, pushConstantStages) {}

ComputePipeline createComputePipeline(vk::raii::Device& device,
                                      const PipelineLayout& layout,
                                      const resource::ShaderLibrary& shaders,
                                      std::string_view spirvPath,
                                      const ComputePipelineSpec& spec) {
    if (spec.entryPoint.empty()) {
        throw std::invalid_argument("ComputePipelineSpec requires an entry point");
    }

    const auto stageInfo = shaders.stage(spirvPath, spec.entryPoint);
    detail::requireStage(stageInfo, vk::ShaderStageFlagBits::eCompute, spec.entryPoint, spirvPath);

    vk::ComputePipelineCreateInfo createInfo{};
    createInfo.setStage(stageInfo).setLayout(layout.getHandle());

    vk::raii::Pipeline pipeline(device, nullptr, createInfo);
    return ComputePipeline(std::move(pipeline),
                           layout.getHandle(),
                           layout.pushConstantStages());
}

}  // namespace render
