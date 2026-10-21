#pragma once

#include <utility>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

namespace render {

// Handle view shared by every pipeline family: owns the vk::Pipeline and borrows
// its PipelineLayout. Creation, fixed-function state and bind point belong to the
// concrete family (GraphicsPipeline / ComputePipeline).
class PipelineBase {
public:
    PipelineBase(const PipelineBase&) = delete;
    PipelineBase& operator=(const PipelineBase&) = delete;
    PipelineBase(PipelineBase&&) noexcept = default;
    PipelineBase& operator=(PipelineBase&&) noexcept = default;
    ~PipelineBase() = default;

    [[nodiscard]] vk::Pipeline binding() const noexcept { return *pipeline_; }
    [[nodiscard]] vk::PipelineLayout layout() const noexcept { return layout_; }
    [[nodiscard]] vk::PipelineBindPoint bindPoint() const noexcept { return bindPoint_; }
    // Union of the stage flags of the layout's push-constant ranges; empty when
    // the pipeline takes no push constants.
    [[nodiscard]] vk::ShaderStageFlags pushConstantStages() const noexcept {
        return pushConstantStages_;
    }

protected:
    PipelineBase(vk::raii::Pipeline&& pipeline,
                 vk::PipelineLayout layout,
                 vk::PipelineBindPoint bindPoint,
                 vk::ShaderStageFlags pushConstantStages)
        : pipeline_(std::move(pipeline)),
          layout_(layout),
          bindPoint_(bindPoint),
          pushConstantStages_(pushConstantStages) {}

    vk::raii::Pipeline    pipeline_;
    vk::PipelineLayout    layout_ = nullptr;
    vk::PipelineBindPoint bindPoint_ = vk::PipelineBindPoint::eGraphics;
    vk::ShaderStageFlags  pushConstantStages_{};
};

}  // namespace render
