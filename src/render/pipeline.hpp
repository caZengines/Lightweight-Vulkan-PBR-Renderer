#pragma once

#include <string_view>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "render/pipeline_spec.hpp"

struct RenderContext;

namespace render {

class ShaderManager;
class DescriptorSetLayout;

// One compiled graphics pipeline described by GraphicsPipelineSpec (Phase 3:
// the formerly hardcoded rasterization/depth/blending knobs became data).
// Shader loading goes through ShaderManager; the depth format arrives inside
// the spec so this class no longer duplicates format probing.
class Pipeline final {
public:
    Pipeline(RenderContext& rct,
             const DescriptorSetLayout& setLayouts,
             const GraphicsPipelineSpec& spec,
             std::string_view spirvPath,
             const ShaderManager& shaders);
    ~Pipeline() = default;

    [[nodiscard]] const vk::raii::Pipeline&       binding() const { return pipeline_; }
    [[nodiscard]] const vk::raii::PipelineLayout& layout()   const { return pipelineLayout_; }

    // Stages covered by the layout's push constant range (empty when the
    // shader declares none); recorders gate pushes on this.
    [[nodiscard]] vk::ShaderStageFlags pushConstantStageFlags() const { return pushConstantStageFlags_; }

private:
    void create(const DescriptorSetLayout& setLayouts,
                const GraphicsPipelineSpec& spec);

    RenderContext&                      rct_;
    std::string_view                    spirvPath_;
    const ShaderManager&                shaders_;
    GraphicsPipelineSpec                spec_{};

    vk::ShaderStageFlags                pushConstantStageFlags_{};
    vk::raii::PipelineLayout            pipelineLayout_ = nullptr;
    vk::raii::Pipeline                  pipeline_       = nullptr;
};

}  // namespace render
