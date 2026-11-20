#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "resource/asset/asset_handle.hpp"
#include "resource/data/vertex.hpp"  // InstanceData (pure data: one mat4 per instance)
#include "scene/transform.hpp"

namespace render {
class InstanceBuffer;
class Material;
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
//
// The material pointer is non-owning: render::MaterialRegistry owns the material
// and must outlive this object.
class SceneObject {
public:
    // mesh: asset handle (must be valid; resolved to MeshGPU here and kept
    // alive by the handle for the object's lifetime). localAABBCenter: the
    // mesh-local AABB center (GltfPrimitive::aabbCenter) used to derive one
    // world-space center per instance. material: registry-owned, non-owning
    // here.
    SceneObject(const resource::AssetHandle& mesh,
                const glm::vec3& localAABBCenter,
                render::Material* material,
                const resource::ResourceRegistry& registry);

    // Uploads the instance matrices (staging + single submit) and snapshots the
    // matching per-instance world AABB centers. The object transform composes
    // on top of every instance placement:
    //   world = transform.toMatrix() * instance.model
    // (an identity transform reproduces the raw instance matrices).
    void setInstances(resource::UploadQueue& queue, std::vector<resource::InstanceData> instances);

    // Marks the world matrix dirty. The GPU instance stream and the snapshot of
    // instance centers are NOT updated automatically — call setInstances() again
    // to refresh both, then Scene::invalidateItems() if this object is already
    // in a Scene.
    void setTransform(const Transform& transform) { transform_ = transform; worldDirty_ = true; }
    [[nodiscard]] const Transform& transform() const { return transform_; }

    // Cached TRS matrix; recomputed only after setTransform (dirty-flag).
    [[nodiscard]] const glm::mat4& worldMatrix() const;

    // World-space AABB center of one instance, snapshotted by setInstances().
    // Precondition: index < instanceCount().
    [[nodiscard]] const glm::vec3& instanceWorldCenter(uint32_t index) const {
        return instanceWorldCenters_[index];
    }
    [[nodiscard]] std::span<const glm::vec3> instanceWorldCenters() const {
        return instanceWorldCenters_;
    }
    // Mean of the per-instance centers; used when one draw covers every
    // instance (opaque materials, whose order does not matter).
    [[nodiscard]] glm::vec3 averageWorldCenter() const;

    [[nodiscard]] const resource::MeshGPU& mesh() const { return *meshGPU_; }
    [[nodiscard]] render::Material& material() const { return *material_; }
    [[nodiscard]] const render::InstanceBuffer& instanceBuffer() const { return *instanceBuffer_; }
    [[nodiscard]] uint32_t instanceCount() const { return instanceCount_; }

private:
    resource::AssetHandle                   meshHandle_;           // keeps the GPU mesh loaded
    const resource::MeshGPU*                meshGPU_ = nullptr;    // registry-owned
    render::Material*                       material_ = nullptr;   // registry-owned
    Transform                               transform_{};
    glm::vec3                               localAABBCenter_{0.0f}; // mesh space
    std::vector<resource::InstanceData>     instances_;            // object-local placements
    std::vector<glm::vec3>                  instanceWorldCenters_; // world space, parallel to instances_
    std::shared_ptr<render::InstanceBuffer> instanceBuffer_;
    uint32_t                                instanceCount_ = 0;

    mutable glm::mat4                       worldMatrix_{1.0f};
    mutable bool                            worldDirty_  = false;
};

}  // namespace scene
