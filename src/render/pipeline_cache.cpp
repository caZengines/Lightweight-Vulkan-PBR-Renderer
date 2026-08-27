#include "render/pipeline_cache.hpp"

#include "render/pipeline.hpp"

namespace render {

const Pipeline& PipelineCache::getOrCreate(
    RenderContext& rct,
    const std::vector<vk::DescriptorSetLayout>& setLayouts,
    const GraphicsPipelineSpec& spec,
    ShaderManager& shaders,
    std::string_view spirvPath) {
    for (const auto& [cachedSpec, pipeline] : entries_) {
        if (cachedSpec == spec) return *pipeline;
    }
    entries_.emplace_back(spec,
                          std::make_unique<Pipeline>(rct, setLayouts, spec, spirvPath, shaders));
    return *entries_.back().second;
}

}  // namespace render
