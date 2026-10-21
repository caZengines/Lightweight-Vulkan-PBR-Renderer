#pragma once

#include <string_view>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "render/pipeline/graphics_pipeline_spec.hpp"
#include "render/pipeline/pipeline_base.hpp"

namespace resource {
class ShaderLibrary;
}  // namespace resource

namespace render {

class PipelineLayout;
class GraphicsPipeline;

[[nodiscard]] GraphicsPipeline createGraphicsPipeline(
    vk::raii::Device& device,
    const PipelineLayout& layout,
    const resource::ShaderLibrary& shaders,
    std::string_view spirvPath,
    const GraphicsPipelineSpec& spec);

// Raster pipeline: vertex/fragment stages plus the fixed-function state described
// by GraphicsPipelineSpec. Owns the vk::Pipeline; borrows its PipelineLayout.
class GraphicsPipeline final : public PipelineBase {
public:
    GraphicsPipeline(const GraphicsPipeline&) = delete;
    GraphicsPipeline& operator=(const GraphicsPipeline&) = delete;
    GraphicsPipeline(GraphicsPipeline&&) noexcept = default;
    GraphicsPipeline& operator=(GraphicsPipeline&&) noexcept = default;
    ~GraphicsPipeline() = default;

private:
    friend GraphicsPipeline createGraphicsPipeline(vk::raii::Device&,
                                                   const PipelineLayout&,
                                                   const resource::ShaderLibrary&,
                                                   std::string_view,
                                                   const GraphicsPipelineSpec&);

    GraphicsPipeline(vk::raii::Pipeline&& pipeline,
                     vk::PipelineLayout layout,
                     vk::ShaderStageFlags pushConstantStages);
};

}  // namespace render
