#include "render/compute/compute_pipeline_library.hpp"

#include "render/pipeline/pipeline_layout_library.hpp"

#include <algorithm>
#include <utility>

namespace render {

ComputePipelineLibrary::ComputePipelineLibrary(vk::raii::Device& device,
                                               const resource::ShaderLibrary& shaders)
    : device_(device), shaders_(shaders) {}

const ComputePipeline& ComputePipelineLibrary::get(const PipelineLayout& layout,
                                                   std::string_view spirvPath,
                                                   const ComputePipelineSpec& spec) {
    Key key{std::string(spirvPath), layout.getHandle(), spec};
    const auto it = std::find_if(
        cache_.begin(), cache_.end(),
        [&](const Entry& entry) { return entry.key == key; });
    if (it != cache_.end()) return it->pipeline;

    ComputePipeline created = createComputePipeline(device_, layout, shaders_, spirvPath, spec);
    cache_.emplace_back(Entry{std::move(key), std::move(created)});
    return cache_.back().pipeline;
}

}  // namespace render
