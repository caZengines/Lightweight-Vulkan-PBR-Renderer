#pragma once

#include <deque>
#include <string>
#include <string_view>

#include "render/pipeline/graphics_pipeline.hpp"

namespace resource {
class ShaderLibrary;
}  // namespace resource

namespace render {

class PipelineLayout;

// Caches raster pipelines by (SPIR-V path, PipelineLayout, spec).
// Returned references remain valid for the lifetime of the library.
class GraphicsPipelineLibrary final {
public:
    GraphicsPipelineLibrary(vk::raii::Device& device, const resource::ShaderLibrary& shaders);
    GraphicsPipelineLibrary(const GraphicsPipelineLibrary&) = delete;
    GraphicsPipelineLibrary& operator=(const GraphicsPipelineLibrary&) = delete;
    GraphicsPipelineLibrary(GraphicsPipelineLibrary&&) = delete;
    GraphicsPipelineLibrary& operator=(GraphicsPipelineLibrary&&) = delete;
    ~GraphicsPipelineLibrary() = default;

    [[nodiscard]] const GraphicsPipeline& get(const PipelineLayout& layout,
                                              std::string_view spirvPath,
                                              const GraphicsPipelineSpec& spec);

private:
    struct Key {
        std::string          spirvPath;
        vk::PipelineLayout   layout = nullptr;
        GraphicsPipelineSpec spec;

        friend bool operator==(const Key&, const Key&) = default;
    };

    struct Entry {
        Key              key;
        GraphicsPipeline pipeline;
    };

    vk::raii::Device&              device_;
    const resource::ShaderLibrary& shaders_;
    std::deque<Entry>              cache_;
};

}  // namespace render
