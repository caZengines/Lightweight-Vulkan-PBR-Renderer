#pragma once

#include <string_view>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "render/pipeline_spec.hpp"

struct RenderContext;

namespace resource {
class ShaderLibrary;
}  // namespace resource

namespace render {

class PipelineLayout;

// One compiled graphics pipeline described by GraphicsPipelineSpec (Phase 3:
// the formerly hardcoded rasterization/depth/blending knobs became data).
//
// Shaders come from resource::ShaderLibrary (stage name per spec.vertEntry/
// fragEntry — no auto-sync of those names, they are the C++↔shader contract) and
// the layout comes from render::PipelineLayoutLibrary.  Pipeline owns neither:
// both outlive it, which is what lets the same layout be shared by the compute
// family later.
class Pipeline final {
public:
    Pipeline(RenderContext& rct,
             const PipelineLayout& layout,
             const resource::ShaderLibrary& shaders,
             std::string_view spirvPath,
             const GraphicsPipelineSpec& spec);
    ~Pipeline() = default;

    [[nodiscard]] vk::Pipeline       binding() const { return pipeline_; }
    [[nodiscard]] vk::PipelineLayout layout()  const { return layoutHandle_; }

    // Stages covered by the layout's push constant ranges (empty when the shader
    // declares none); recorders gate pushes on this.  Union over all ranges —
    // taking only ranges.front() silently drops stages when a second range is
    // added (the old B1 defect).
    [[nodiscard]] vk::ShaderStageFlags pushConstantStageFlags() const { return pushConstantStageFlags_; }

private:
    void create(const resource::ShaderLibrary& shaders,
                std::string_view spirvPath,
                const GraphicsPipelineSpec& spec);

    RenderContext&                rct_;
    vk::PipelineLayout            layoutHandle_ = nullptr;
    vk::ShaderStageFlags          pushConstantStageFlags_{};
    vk::raii::Pipeline            pipeline_ = nullptr;
};

}  // namespace render
