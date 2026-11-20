#include "scene/scene_object.hpp"

#include <vulkan/vulkan.hpp>
#include <stdexcept>

#include "render/gpu/instance_buffer.hpp"
#include "render/material/material.hpp"
#include "resource/asset/resource_registry.hpp"
#include "resource/gpu/upload_queue.hpp"

namespace scene {

SceneObject::SceneObject(const resource::AssetHandle& mesh,
                         const glm::vec3& localAABBCenter,
                         render::Material* material,
                         const resource::ResourceRegistry& registry)
    : meshHandle_(mesh), material_(material), localAABBCenter_(localAABBCenter)
{
    if (!mesh.valid()) {
        throw std::runtime_error("SceneObject: null mesh handle (asset not loaded)");
    }
    if (!material_) {
        throw std::runtime_error("SceneObject: null material (must be registry-owned)");
    }
    meshGPU_ = &registry.mesh(mesh);
}

void SceneObject::setInstances(resource::UploadQueue& queue, std::vector<resource::InstanceData> instances) {
    instances_ = std::move(instances);
    instanceCount_ = static_cast<uint32_t>(instances_.size());

    // Compose the object transform on top of each instance placement. The
    // stored placements stay object-local so re-uploads never double-apply.
    // The per-instance center is snapshotted here so it always matches the
    // matrices that are about to be uploaded.
    const glm::mat4 objectWorld = worldMatrix();

    std::vector<resource::InstanceData> worldInstances;
    worldInstances.reserve(instances_.size());
    instanceWorldCenters_.resize(instances_.size());
    for (size_t i = 0; i < instances_.size(); ++i) {
        const glm::mat4 model = objectWorld * instances_[i].model;
        worldInstances.emplace_back(resource::InstanceData{model});
        instanceWorldCenters_[i] =
            glm::vec3(model * glm::vec4(localAABBCenter_, 1.0f));
    }
    instanceBuffer_ = std::make_shared<render::InstanceBuffer>(queue, worldInstances);
}

const glm::mat4& SceneObject::worldMatrix() const {
    if (worldDirty_) {
        worldMatrix_ = transform_.toMatrix();
        worldDirty_ = false;
    }
    return worldMatrix_;
}

glm::vec3 SceneObject::averageWorldCenter() const {
    if (instanceWorldCenters_.empty()) {
        return glm::vec3(0.0f);
    }
    glm::vec3 sum(0.0f);
    for (const glm::vec3& center : instanceWorldCenters_) {
        sum += center;
    }
    return sum / static_cast<float>(instanceWorldCenters_.size());
}

}  // namespace scene
