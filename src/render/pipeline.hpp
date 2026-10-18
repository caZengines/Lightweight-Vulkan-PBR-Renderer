#pragma once

#include <string_view>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "render/pipeline_spec.hpp"

namespace resource {
class ShaderLibrary;
}  // namespace resource

namespace render {

class PipelineLayout;

class Pipeline;
[[nodiscard]] Pipeline createGraphicsPipeline(
    vk::raii::Device& device,
    const PipelineLayout& layout,
    const resource::ShaderLibrary& shaders,
    std::string_view spirvPath,
    const GraphicsPipelineSpec& spec);
[[nodiscard]] Pipeline createComputePipeline(
    vk::raii::Device& device,
    const PipelineLayout& layout,
    const resource::ShaderLibrary& shaders,
    std::string_view spirvPath,
    const ComputePipelineSpec& spec);

// Owns one compiled Vulkan pipeline; borrows its PipelineLayout.
class Pipeline final {
public:
    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;
    Pipeline(Pipeline&&) noexcept = default;
    Pipeline& operator=(Pipeline&&) noexcept = default;
    ~Pipeline() = default;

    [[nodiscard]] vk::Pipeline binding() const noexcept { return *pipeline_; }
    [[nodiscard]] vk::PipelineLayout layout() const noexcept { return layout_; }
    [[nodiscard]] vk::PipelineBindPoint bindPoint() const noexcept { return bindPoint_; }
    [[nodiscard]] vk::ShaderStageFlags pushConstantStages() const noexcept {
        return pushConstantStages_;
    }

private:
    friend Pipeline createGraphicsPipeline(vk::raii::Device&,
                                           const PipelineLayout&,
                                           const resource::ShaderLibrary&,
                                           std::string_view,
                                           const GraphicsPipelineSpec&);
    friend Pipeline createComputePipeline(vk::raii::Device&,
                                          const PipelineLayout&,
                                          const resource::ShaderLibrary&,
                                          std::string_view,
                                          const ComputePipelineSpec&);

    Pipeline(vk::raii::Pipeline&& pipeline,
             vk::PipelineLayout layout,
             vk::PipelineBindPoint bindPoint,
             vk::ShaderStageFlags pushConstantStages);

    vk::raii::Pipeline    pipeline_;
    vk::PipelineLayout    layout_ = nullptr;
    vk::PipelineBindPoint bindPoint_ = vk::PipelineBindPoint::eGraphics;
    vk::ShaderStageFlags  pushConstantStages_{};
};

}  // namespace render
