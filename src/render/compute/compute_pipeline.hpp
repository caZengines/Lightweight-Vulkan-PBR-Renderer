#pragma once

#include <string_view>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "render/compute/compute_pipeline_spec.hpp"
#include "render/pipeline/pipeline_base.hpp"

namespace resource {
class ShaderLibrary;
}  // namespace resource

namespace render {

class PipelineLayout;
class ComputePipeline;

[[nodiscard]] ComputePipeline createComputePipeline(
    vk::raii::Device& device,
    const PipelineLayout& layout,
    const resource::ShaderLibrary& shaders,
    std::string_view spirvPath,
    const ComputePipelineSpec& spec);

// Dispatch pipeline: one compute stage, no fixed-function state. Owns the
// vk::Pipeline; borrows its PipelineLayout.
class ComputePipeline final : public PipelineBase {
public:
    ComputePipeline(const ComputePipeline&) = delete;
    ComputePipeline& operator=(const ComputePipeline&) = delete;
    ComputePipeline(ComputePipeline&&) noexcept = default;
    ComputePipeline& operator=(ComputePipeline&&) noexcept = default;
    ~ComputePipeline() = default;

private:
    friend ComputePipeline createComputePipeline(vk::raii::Device&,
                                                 const PipelineLayout&,
                                                 const resource::ShaderLibrary&,
                                                 std::string_view,
                                                 const ComputePipelineSpec&);

    ComputePipeline(vk::raii::Pipeline&& pipeline,
                    vk::PipelineLayout layout,
                    vk::ShaderStageFlags pushConstantStages);
};

}  // namespace render
