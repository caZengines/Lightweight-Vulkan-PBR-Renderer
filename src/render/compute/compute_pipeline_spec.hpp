#pragma once

#include <string>

namespace render {

// Value-type description of one compute pipeline.
struct ComputePipelineSpec {
    std::string entryPoint = "csMain";

    friend bool operator==(const ComputePipelineSpec&,
                           const ComputePipelineSpec&) = default;
};

}  // namespace render
