#pragma once

#include <deque>
#include <string>
#include <string_view>

#include "render/compute/compute_pipeline.hpp"

namespace resource {
class ShaderLibrary;
}  // namespace resource

namespace render {

class PipelineLayout;

// Caches compute pipelines by (SPIR-V path, PipelineLayout, spec).
// Returned references remain valid for the lifetime of the library.
class ComputePipelineLibrary final {
public:
    ComputePipelineLibrary(vk::raii::Device& device, const resource::ShaderLibrary& shaders);
    ComputePipelineLibrary(const ComputePipelineLibrary&) = delete;
    ComputePipelineLibrary& operator=(const ComputePipelineLibrary&) = delete;
    ComputePipelineLibrary(ComputePipelineLibrary&&) = delete;
    ComputePipelineLibrary& operator=(ComputePipelineLibrary&&) = delete;
    ~ComputePipelineLibrary() = default;

    [[nodiscard]] const ComputePipeline& get(const PipelineLayout& layout,
                                             std::string_view spirvPath,
                                             const ComputePipelineSpec& spec);

private:
    struct Key {
        std::string         spirvPath;
        vk::PipelineLayout  layout = nullptr;
        ComputePipelineSpec spec;

        friend bool operator==(const Key&, const Key&) = default;
    };

    struct Entry {
        Key             key;
        ComputePipeline pipeline;
    };

    vk::raii::Device&              device_;
    const resource::ShaderLibrary& shaders_;
    std::deque<Entry>              cache_;
};

}  // namespace render
