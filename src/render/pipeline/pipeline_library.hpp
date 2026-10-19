#pragma once

#include <deque>
#include <string>
#include <string_view>

#include "render/pipeline/pipeline.hpp"

namespace resource {
class ShaderLibrary;
}  // namespace resource

namespace render {

class PipelineLayout;

// Caches compiled pipelines by (SPIR-V path, PipelineLayout, spec).
// Returned references remain valid for the lifetime of the library.
class PipelineLibrary final {
public:
    PipelineLibrary(vk::raii::Device& device, const resource::ShaderLibrary& shaders);
    PipelineLibrary(const PipelineLibrary&) = delete;
    PipelineLibrary& operator=(const PipelineLibrary&) = delete;
    PipelineLibrary(PipelineLibrary&&) = delete;
    PipelineLibrary& operator=(PipelineLibrary&&) = delete;
    ~PipelineLibrary() = default;

    [[nodiscard]] const Pipeline& graphics(const PipelineLayout& layout,
                                           std::string_view spirvPath,
                                           const GraphicsPipelineSpec& spec);
    [[nodiscard]] const Pipeline& compute(const PipelineLayout& layout,
                                          std::string_view spirvPath,
                                          const ComputePipelineSpec& spec);

private:
    struct GraphicsKey {
        std::string          spirvPath;
        vk::PipelineLayout   layout = nullptr;
        GraphicsPipelineSpec spec;

        friend bool operator==(const GraphicsKey&, const GraphicsKey&) = default;
    };

    struct ComputeKey {
        std::string         spirvPath;
        vk::PipelineLayout  layout = nullptr;
        ComputePipelineSpec spec;

        friend bool operator==(const ComputeKey&, const ComputeKey&) = default;
    };

    struct GraphicsEntry {
        GraphicsKey key;
        Pipeline    pipeline;
    };

    struct ComputeEntry {
        ComputeKey key;
        Pipeline   pipeline;
    };

    vk::raii::Device&              device_;
    const resource::ShaderLibrary& shaders_;
    std::deque<GraphicsEntry>      graphics_;
    std::deque<ComputeEntry>       compute_;
};

}  // namespace render
