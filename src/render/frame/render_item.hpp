#pragma once

#include <cstdint>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

namespace resource {
class MeshGPU;
}  // namespace resource


namespace render {

class InstanceBuffer;
class Material;

// Pure-data draw unit: scene::Scene fills these, RasterPass consumes them.
// Flat POD with no Vulkan types; instance streams use render::InstanceBuffer.
//
// Blend materials are split into one item per instance (instanceCount == 1) by
// the scene, so the renderer can reorder the draws back-to-front. Opaque items
// may cover several instances because their draw order does not matter.
struct RenderItem {
    const resource::MeshGPU*           mesh      = nullptr;
    const Material*                    material  = nullptr;
    const InstanceBuffer*              instances = nullptr;
    uint32_t                           firstInstance = 0;
    uint32_t                           instanceCount = 0;
    glm::vec3                          worldCenter{};  // world-space AABB center of the drawn range
};

}  // namespace render
