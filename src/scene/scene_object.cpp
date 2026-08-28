#include "scene/scene_object.hpp"

#include <stdexcept>

#include "resource/material.hpp"
#include "render/instance_buffer.hpp"
#include "resource/resource_registry.hpp"
#include "resource/upload_queue.hpp"

namespace scene {

SceneObject::SceneObject(const resource::AssetHandle& mesh,
                         std::shared_ptr<Material> material,
                         const resource::ResourceRegistry& registry)
    : meshHandle_(mesh), material_(std::move(material))
{
    if (!mesh.valid()) {
        throw std::runtime_error("SceneObject: null mesh handle (asset not loaded)");
    }
    meshGPU_ = &registry.mesh(mesh);
}

void SceneObject::setInstances(resource::UploadQueue& queue, std::vector<rhi::InstanceData> instances) {
    instances_ = std::move(instances);
    instanceCount_ = static_cast<uint32_t>(instances_.size());

    // Compose the object transform on top of each instance placement; the
    // stored placements stay object-local so re-uploads never double-apply.
    const glm::mat4 world = worldMatrix();
    std::vector<rhi::InstanceData> worldInstances;
    worldInstances.reserve(instances_.size());
    for (const rhi::InstanceData& local : instances_) {
        worldInstances.emplace_back(rhi::InstanceData{world * local.model});
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

}  // namespace scene
