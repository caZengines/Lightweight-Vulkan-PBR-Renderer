#pragma once

#include <cstdint>

namespace resource {
class MeshGPU;
}  // namespace resource


namespace render {

class InstanceBuffer;
class Material;

// Pure-data draw unit: scene::Scene fills these, RasterPass consumes them.
// Flat POD with no Vulkan types; instance streams use render::InstanceBuffer.
struct RenderItem {
    const resource::MeshGPU*           mesh      = nullptr;
    const Material*                    material  = nullptr;
    const InstanceBuffer*              instances = nullptr;
    uint32_t                           firstInstance = 0;
    uint32_t                           instanceCount = 0;
};

}  // namespace render
