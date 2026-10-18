#include "render/pipeline_library.hpp"

#include "render/pipeline_layout_library.hpp"

#include <algorithm>
#include <utility>

namespace render {

PipelineLibrary::PipelineLibrary(vk::raii::Device& device,
                                 const resource::ShaderLibrary& shaders)
    : device_(device), shaders_(shaders) {}

const Pipeline& PipelineLibrary::graphics(const PipelineLayout& layout,
                                          std::string_view spirvPath,
                                          const GraphicsPipelineSpec& spec) {
    GraphicsKey key{std::string(spirvPath), layout.getHandle(), spec};
    const auto it = std::find_if(
        graphics_.begin(), graphics_.end(),
        [&](const GraphicsEntry& entry) { return entry.key == key; });
    if (it != graphics_.end()) return it->pipeline;

    Pipeline created = createGraphicsPipeline(device_, layout, shaders_, spirvPath, spec);
    graphics_.push_back(GraphicsEntry{std::move(key), std::move(created)});
    return graphics_.back().pipeline;
}

const Pipeline& PipelineLibrary::compute(const PipelineLayout& layout,
                                         std::string_view spirvPath,
                                         const ComputePipelineSpec& spec) {
    ComputeKey key{std::string(spirvPath), layout.getHandle(), spec};
    const auto it = std::find_if(
        compute_.begin(), compute_.end(),
        [&](const ComputeEntry& entry) { return entry.key == key; });
    if (it != compute_.end()) return it->pipeline;

    Pipeline created = createComputePipeline(device_, layout, shaders_, spirvPath, spec);
    compute_.push_back(ComputeEntry{std::move(key), std::move(created)});
    return compute_.back().pipeline;
}

}  // namespace render
