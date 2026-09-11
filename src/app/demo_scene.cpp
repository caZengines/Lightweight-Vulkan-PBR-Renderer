#include "app/demo_scene.hpp"

#include "app/config.hpp"
#include "platform/log.hpp"
#include "resource/gltf_importer.hpp"
#include "resource/material.hpp"
#include "resource/sampler.hpp"
#include "rhi/vertex.hpp"
#include "resource/asset_library.hpp"
#include "resource/resource_registry.hpp"
#include "resource/upload_queue.hpp"
#include "scene/scene.hpp"

#include <limits>
#include <random>

namespace app {

DemoScene::DemoScene(const Config& config, scene::Scene& scene)
    : config_(config), scene_(scene)
{
    // Demo light — the exact value the renderer used to hardcode, now owned by
    // the content. Projection params are per-camera (scene::Camera defaults).
    frameParams_ = render::FrameParams{
        .light{
            .pos       = glm::vec4(4.0f, 20.0f, -25.0f, 1.0f),
            .color     = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),
            .intensity = 10.0f,
        },
    };
}

void DemoScene::build(const Sampler& albedoSampler, const Sampler& normalSampler,
                      resource::AssetLibrary& assets, resource::ResourceRegistry& registry,
                      resource::UploadQueue& queue) {

    buildCar(albedoSampler, normalSampler, assets, registry, queue);
}

void DemoScene::buildCar(const Sampler& albedoSampler, const Sampler& normalSampler,
                         resource::AssetLibrary& assets, resource::ResourceRegistry& registry,
                         resource::UploadQueue& queue) {
    // --- glTF car scene: one SceneObject per primitive ---
    // Geometry is imported in local space with the node hierarchy collapsed
    // into GltfPrimitive::world; that matrix rides in the (single) instance
    // placement, leaving the object transform at identity.
    const resource::GltfScene carScene = resource::GltfSceneImporter::load(config_.modelPath);

    // Transitional: one shared default material for the whole car (empty
    // handles → built-in 1×1 fallback textures = white model). Per-primitive
    // materials keyed by GltfPrimitive::materialIndex come with the
    // material/texture work.
    auto defaultMaterial = std::make_shared<Material>(resource::AssetHandle{},
                                                      resource::AssetHandle{},
                                                      albedoSampler, normalSampler, registry);
    materials_.emplace_back(defaultMaterial);

    const glm::mat4 carPlacement =
        glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f));

    glm::vec3 aabbMin(std::numeric_limits<float>::max());
    glm::vec3 aabbMax(std::numeric_limits<float>::lowest());
    size_t totalVertices = 0;
    for (size_t i = 0; i < carScene.primitives.size(); ++i) {
        const resource::GltfPrimitive& prim = carScene.primitives[i];
        const glm::mat4 modelMatrix = carPlacement * prim.world;

        // World-space bounds (logged so the orbit camera can be aimed by hand).
        for (const rhi::Vertex& v : prim.mesh.vertices()) {
            const glm::vec3 wp = glm::vec3(modelMatrix * glm::vec4(v.pos, 1.0f));
            aabbMin = glm::min(aabbMin, wp);
            aabbMax = glm::max(aabbMax, wp);
        }
        totalVertices += prim.mesh.vertices().size();

        const std::string key = config_.modelPath + "#prim" + std::to_string(i);
        auto meshHandle = assets.loadMeshData(key, prim.mesh);

        auto object = std::make_shared<scene::SceneObject>(meshHandle, defaultMaterial, registry);
        std::vector<rhi::InstanceData> instances(1);
        instances[0].model = modelMatrix;
        object->setInstances(queue, std::move(instances));
        scene_.addObject(std::move(object));
    }

    const glm::vec3 center = (aabbMin + aabbMax) * 0.5f;
    platform::LogLocator::get().write(platform::LogLevel::Info,
        "DemoScene: car '" + config_.modelPath + "': " +
        std::to_string(carScene.primitives.size()) + " primitives, " +
        std::to_string(totalVertices) + " vertices, AABB min(" +
        std::to_string(aabbMin.x) + ", " + std::to_string(aabbMin.y) + ", " + std::to_string(aabbMin.z) +
        ") max(" + std::to_string(aabbMax.x) + ", " + std::to_string(aabbMax.y) + ", " +
        std::to_string(aabbMax.z) + ") center(" +
        std::to_string(center.x) + ", " + std::to_string(center.y) + ", " + std::to_string(center.z) + ")");
}

}  // namespace app
