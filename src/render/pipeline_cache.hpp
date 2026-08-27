#pragma once

#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "render/pipeline_spec.hpp"

struct RenderContext;

namespace render {

class Pipeline;
class ShaderManager;

// Builds pipelines on demand keyed by GraphicsPipelineSpec. Kept as a small
// linear vector on purpose: realistic scenes hold a handful of distinct specs,
// so linear search beats hashing ceremony here.
class PipelineCache final {
public:
    [[nodiscard]] const Pipeline& getOrCreate(
        RenderContext& rct,
        const std::vector<vk::DescriptorSetLayout>& setLayouts,
        const GraphicsPipelineSpec& spec,
        ShaderManager& shaders,
        std::string_view spirvPath);

private:
    std::vector<std::pair<GraphicsPipelineSpec, std::unique_ptr<Pipeline>>> entries_;
};

}  // namespace render
