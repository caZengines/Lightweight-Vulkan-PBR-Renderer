#include "app/demo_scene.hpp"

#include "app/config.hpp"
#include "platform/log.hpp"
#include "resource/gltf_importer.hpp"
#include "resource/material.hpp"
#include "resource/sampler.hpp"
#include "rhi/vertex.hpp"
#include "resource/asset_library.hpp"
#include "resource/resource_registry.hpp"
#include "resource/texture_importer.hpp"
#include "resource/upload_queue.hpp"
#include "scene/scene.hpp"

#include <limits>

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

    buildglTFdemo(albedoSampler, normalSampler, assets, registry, queue);
}

void DemoScene::buildglTFdemo(const Sampler& albedoSampler, const Sampler& normalSampler,
                         resource::AssetLibrary& assets, resource::ResourceRegistry& registry,
                         resource::UploadQueue& queue) {
    // --- glTF scene: one SceneObject per primitive, one Material per glTF
    // material. Geometry is imported in local space with the node hierarchy
    // collapsed into GltfPrimitive::world; that matrix rides in the (single)
    // instance placement, leaving the object transform at identity.
    const resource::GltfScene imported = resource::GltfSceneImporter::load(config_.modelPath);

    // materials_[0] is the fallback for primitives without a material;
    // glTF material i lives at materials_[i + 1].
    materials_.reserve(imported.materials.size() + 1);
    materials_.emplace_back(std::make_shared<Material>(resource::AssetHandle{},
                                                    resource::AssetHandle{},
                                                    resource::AssetHandle{},
                                                    resource::AssetHandle{},
                                                    resource::AssetHandle{},
                                                    albedoSampler, normalSampler, registry));
    for (const resource::MaterialData& data : imported.materials) {
        materials_.push_back(std::make_shared<Material>(
            resolveSlotTexture(imported, data, resource::MaterialTextureSlot::BaseColor, assets),
            resolveSlotTexture(imported, data, resource::MaterialTextureSlot::MetallicRoughness, assets),
            resolveSlotTexture(imported, data, resource::MaterialTextureSlot::Normal, assets),
            resolveSlotTexture(imported, data, resource::MaterialTextureSlot::Occlusion, assets),
            resolveSlotTexture(imported, data, resource::MaterialTextureSlot::Emissive, assets),
            albedoSampler, normalSampler, registry));
    }

    glm::mat4 placement = glm::scale(glm::mat4(1.0f), glm::vec3(0.1f, 0.1f, 0.1f));

    glm::vec3 aabbMin(std::numeric_limits<float>::max());
    glm::vec3 aabbMax(std::numeric_limits<float>::lowest());
    size_t totalVertices = 0;
    for (size_t i = 0; i < imported.primitives.size(); ++i) {
        const resource::GltfPrimitive& prim = imported.primitives[i];
        const glm::mat4 modelMatrix = placement * prim.world;

        // World-space bounds (logged so the orbit camera can be aimed by hand).
        for (const rhi::Vertex& v : prim.mesh.vertices()) {
            const glm::vec3 wp = glm::vec3(modelMatrix * glm::vec4(v.pos, 1.0f));
            aabbMin = glm::min(aabbMin, wp);
            aabbMax = glm::max(aabbMax, wp);
        }
        totalVertices += prim.mesh.vertices().size();

        const std::string key = config_.modelPath + "#prim" + std::to_string(i);
        auto meshHandle = assets.loadMeshData(key, prim.mesh);

        const std::shared_ptr<Material>& material =
            prim.materialIndex < imported.materials.size()
                ? materials_[prim.materialIndex + 1]
                : materials_[0];
        auto object = std::make_shared<scene::SceneObject>(meshHandle, material, registry);
        std::vector<rhi::InstanceData> instances(1);
        instances[0].model = modelMatrix;
        object->setInstances(queue, std::move(instances));
        scene_.addObject(std::move(object));
    }

    const glm::vec3 center = (aabbMin + aabbMax) * 0.5f;
    platform::LogLocator::get().write(platform::LogLevel::Info,
        "DemoScene: '" + config_.modelPath + "': " +
        std::to_string(imported.primitives.size()) + " primitives, " +
        std::to_string(imported.materials.size()) + " materials, " +
        std::to_string(totalVertices) + " vertices, AABB min(" +
        std::to_string(aabbMin.x) + ", " + std::to_string(aabbMin.y) + ", " + std::to_string(aabbMin.z) +
        ") max(" + std::to_string(aabbMax.x) + ", " + std::to_string(aabbMax.y) + ", " +
        std::to_string(aabbMax.z) + ") center(" +
        std::to_string(center.x) + ", " + std::to_string(center.y) + ", " + std::to_string(center.z) + ")");
}

resource::AssetHandle DemoScene::resolveSlotTexture(
    const resource::GltfScene& imported,
    const resource::MaterialData& material,
    resource::MaterialTextureSlot slot,
    resource::AssetLibrary& assets) const {
    const resource::TextureSlot& textureSlot =
        material.slots[static_cast<size_t>(slot)];
    if (textureSlot.texture < 0 ||
        static_cast<size_t>(textureSlot.texture) >= imported.textures.size()) {
        return {};  // Material maps an empty handle to its default texture.
    }
    const resource::TextureSource& source =
        imported.textures[static_cast<size_t>(textureSlot.texture)];
    if (!source.valid) {
        return {};
    }

    // Base color/emissive are colors (sRGB); the rest are data (linear).
    const bool colorMap = slot == resource::MaterialTextureSlot::BaseColor ||
                          slot == resource::MaterialTextureSlot::Emissive;
    const vk::Format format = colorMap ? vk::Format::eR8G8B8A8Srgb
                                       : vk::Format::eR8G8B8A8Unorm;

    if (!source.embedded) {
        return assets.loadImage(source.uri, format, vk::Filter::eLinear);
    }
    resource::ImageData image =
        resource::TextureImporter::loadFromMemory(source.bytes.data(), source.bytes.size());
    const std::string key = config_.modelPath + "#img" + std::to_string(source.image);
    return assets.loadImageData(key, image, format, vk::Filter::eLinear);
}

}  // namespace app
