#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "generic/vertex.hpp"  // InstanceData (pure data: one mat4 per instance)
#include "resource/asset_handle.hpp"
#include "scene/transform.hpp"

class Material;  // transitional: GPU-backed until materials become pure data

namespace render {
class InstanceBuffer;
}  // namespace render

namespace resource {
class MeshGPU;
class ResourceRegistry;
class UploadQueue;
}  // namespace resource

namespace scene {

// Pure-data scene object: what to draw (mesh + material), where (transform),
// and per-instance placements. Names no Vulkan types — the GPU-side instance
// stream is a render::InstanceBuffer, created through the resource layer's
// UploadQueue at setInstances() time.
class SceneObject {
public:
    // mesh: asset handle (must be valid; resolved to MeshGPU here and kept
    // alive by the handle for the object's lifetime).
    SceneObject(const resource::AssetHandle& mesh,
                std::shared_ptr<Material> material,
                const resource::ResourceRegistry& registry);

    // Uploads the instance matrices (staging + single submit). The object
    // transform composes on top of every instance placement:
    //   world = transform.toMatrix() * instance.model
    // (an identity transform reproduces the raw instance matrices).
    void setInstances(resource::UploadQueue& queue, std::vector<InstanceData> instances);

    // Marks the world matrix dirty. The GPU instance stream is NOT re-uploaded
    // automatically — call setInstances() again to push updated matrices.
    void setTransform(const Transform& transform) { transform_ = transform; worldDirty_ = true; }
    [[nodiscard]] const Transform& transform() const { return transform_; }

    // Cached TRS matrix; recomputed only after setTransform (dirty-flag).
    [[nodiscard]] const glm::mat4& worldMatrix() const;

    [[nodiscard]] const resource::MeshGPU& mesh() const { return *meshGPU_; }
    [[nodiscard]] Material& material() const { return *material_; }
    [[nodiscard]] const render::InstanceBuffer& instanceBuffer() const { return *instanceBuffer_; }
    [[nodiscard]] uint32_t instanceCount() const { return instanceCount_; }

private:
    resource::AssetHandle          meshHandle_;         // keeps the GPU mesh loaded
    const resource::MeshGPU*       meshGPU_ = nullptr;  // registry-owned
    std::shared_ptr<Material>      material_;
    Transform                      transform_{};
    std::vector<InstanceData>      instances_;          // object-local placements
    std::shared_ptr<render::InstanceBuffer> instanceBuffer_;
    uint32_t                       instanceCount_ = 0;

    mutable glm::mat4              worldMatrix_{1.0f};
    mutable bool                   worldDirty_  = false;
};

}  // namespace scene
