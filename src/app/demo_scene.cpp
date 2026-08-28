#include "app/demo_scene.hpp"

#include "app/config.hpp"
#include "resource/material.hpp"
#include "resource/sampler.hpp"
#include "rhi/vertex.hpp"
#include "resource/asset_library.hpp"
#include "resource/resource_registry.hpp"
#include "resource/upload_queue.hpp"
#include "scene/scene.hpp"

#include <random>

namespace app {

DemoScene::DemoScene(const Config& config, scene::Scene& scene)
    : config_(config), scene_(scene)
{
    // Demo light + camera projection — the exact values the renderer used to
    // hardcode, now owned by the content.
    frameParams_ = render::FrameParams{
        .light{
            .pos       = glm::vec4(4.0f, 20.0f, -25.0f, 1.0f),
            .color     = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),
            .intensity = 10.0f,
        },
        .fovDegrees = 45.0f,
        .nearPlane  = 0.1f,
        .farPlane   = 100.0f,
    };
}

void DemoScene::build(const Sampler& albedoSampler, const Sampler& normalSampler,
                      resource::AssetLibrary& assets, resource::ResourceRegistry& registry,
                      resource::UploadQueue& queue) {
    // --- materials ---
    // Get-or-load textures; the returned handles are kept by the Materials
    // (refcounted), so duplicate loads never re-upload.
    auto rockAlbedo = assets.loadImage(config_.rockTexturePath, vk::Format::eR8G8B8A8Srgb, vk::Filter::eLinear);
    auto marsAlbedo = assets.loadImage(config_.marsTexturePath, vk::Format::eR8G8B8A8Srgb, vk::Filter::eLinear);

    // Empty (null) texture handles fall back to the registry's built-in
    // default textures (Null Object semantics).
    materials_.emplace_back(std::make_shared<Material>(resource::AssetHandle{}, resource::AssetHandle{},
                                                    albedoSampler, normalSampler, registry));
    materials_.emplace_back(std::make_shared<Material>(marsAlbedo, resource::AssetHandle{},
                                                    albedoSampler, normalSampler, registry));
    materials_.emplace_back(std::make_shared<Material>(rockAlbedo, resource::AssetHandle{},
                                                    albedoSampler, normalSampler, registry));
    const auto& defaultMaterial = materials_[0];
    const auto& marsMaterial    = materials_[1];
    const auto& rockMaterial    = materials_[2];

    // --- mars ---
    auto marsMeshHandle = assets.loadMesh(config_.planetPath);
    auto mars = std::make_shared<scene::SceneObject>(marsMeshHandle, marsMaterial, registry);
    std::vector<rhi::InstanceData> marsInstances(1);
    glm::mat4 marsModel = glm::mat4(1.0f);
    marsModel = glm::translate(marsModel, glm::vec3(0.0f, -3.0f, 0.0f));
    marsModel = glm::scale(marsModel, glm::vec3(2.0f, 2.0f, 2.0f));
    marsInstances[0].model = marsModel;
    mars->setInstances(queue, std::move(marsInstances));
    scene_.addObject(std::move(mars));

    // --- 1000 rocks in a randomized ring ---
    auto rockMeshHandle = assets.loadMesh(config_.rockPath);
    auto rock = std::make_shared<scene::SceneObject>(rockMeshHandle, rockMaterial, registry);
    const uint32_t amount = 1000;
    const float radius = 40.0f;
    const float offset = 2.5f;
    std::vector<rhi::InstanceData> rocks(amount);
    std::random_device rd;
    std::mt19937 gen(rd());

    std::uniform_real_distribution<float> radialDist(-offset, offset);
    std::normal_distribution<float> heightDist(0.0f, 2.0f);              // Vertical thickness（Gauss）
    std::uniform_real_distribution<float> angleDist(0.0f, 360.0f);       // rotation angle
    std::uniform_real_distribution<float> axisDist(-1.0f, 1.0f);         // random axis
    std::uniform_real_distribution<float> scaleDist(0.05, 0.25);         // scale
    std::uniform_real_distribution<float> phaseDist(0.0f, 360.0f);

    for (size_t i = 0; i < amount; ++i) {
        glm::mat4 model = glm::mat4(1.0f);

        const float angle = (360.0f / amount) * i + phaseDist(gen);
        const float r = radius + radialDist(gen);
        const float x = sin(glm::radians(angle)) * r;
        const float z = cos(glm::radians(angle)) * r;
        const float y = heightDist(gen);
        model = glm::translate(model, glm::vec3(x, y, z));

        const glm::vec3 axis = glm::normalize(glm::vec3(
            axisDist(gen),
            axisDist(gen),
            axisDist(gen)
        ));
        const float rotAngle = angleDist(gen);
        model = glm::rotate(model, glm::radians(rotAngle), axis);
        const float s = scaleDist(gen);
        model = glm::scale(model, glm::vec3(s));

        rocks[i].model = model;
    }
    rock->setInstances(queue, std::move(rocks));
    scene_.addObject(std::move(rock));
}

}  // namespace app
