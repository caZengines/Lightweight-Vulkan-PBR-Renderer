#pragma once

#include <cstdint>

namespace resource {
class MeshGPU;
class Material;
}  // namespace resource


namespace render {

class InstanceBuffer;

// Pure-data draw unit: scene::Scene fills these, CommandRecorder consumes
// them. Flat POD on purpose — zero virtuals and no pointer chasing in the hot
// recording loop (plan §2.6, data-oriented design). No Vulkan types: instance
// streams are referenced through render::InstanceBuffer.
//
// Transitional shape: raw mesh/material pointers serve the current raster
// path; the planned ray-tracing pipeline (post-refactor, see plan doc §6)
// replaces draw items with TLAS instances built from the same scene data.
struct RenderItem {
    const resource::MeshGPU*           mesh      = nullptr;
    const resource::Material*          material  = nullptr;
    const InstanceBuffer*              instances = nullptr;
    uint32_t                           firstInstance = 0;
    uint32_t                           instanceCount = 0;
};

}  // namespace render
