#include "app/demo_scene.hpp"

#include "app/config.hpp"
#include "platform/log.hpp"
#include "resource/import/gltf_importer.hpp"
#include "render/material/material.hpp"
#include "render/material/material_registry.hpp"
#include "resource/gpu/sampler.hpp"
#include "resource/data/vertex.hpp"
#include "resource/asset/asset_library.hpp"
#include "resource/asset/resource_registry.hpp"
#include "resource/import/texture_importer.hpp"
#include "resource/gpu/upload_queue.hpp"
#include "scene/scene.hpp"

#include <memory>
#include <string>
#include <vector>

namespace app {

DemoScene::DemoScene(const Config& config, scene::Scene& scene)
    : config_(config), scene_(scene)
{
    // Demo light, owned by the content and handed to the renderer via
    // FrameParams.  Projection params are per-camera (scene::Camera defaults).
    frameParams_ = render::FrameParams{
        .light{
            .pos       = glm::vec4(2.0f, 6.0f, 0.0f, 1.0f),
            .color     = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),
            .intensity = 100.0f,
        },
    };
}

void DemoScene::build(const Sampler& albedoSampler, const Sampler& normalSampler,
                      resource::AssetLibrary& assets, resource::ResourceRegistry& registry,
                      resource::UploadQueue& queue, render::MaterialRegistry& materials) {

    buildglTFdemo(albedoSampler, normalSampler, assets, registry, queue, materials);
}

void DemoScene::buildglTFdemo(const Sampler& albedoSampler, const Sampler& normalSampler,
                         resource::AssetLibrary& assets, resource::ResourceRegistry& registry,
                         resource::UploadQueue& queue, render::MaterialRegistry& materials) {
    // --- glTF scene: one SceneObject per primitive, one Material per glTF
    // material. Geometry is imported in local space with the node hierarchy
    // collapsed into GltfPrimitive::world; that matrix rides in the (single)
    // instance placement, leaving the object transform at identity.
    const resource::GltfScene imported = resource::GltfSceneImporter::load(config_.modelPath);

    // The registry owns every material: the first entry is the fallback for
    // primitives without a material, glTF material i is registered at index
    // i + 1 (ids follow creation order).
    std::vector<render::Material*> materialsByIndex;
    materialsByIndex.reserve(imported.materials.size() + 1);
    materialsByIndex.emplace_back(&materials.create(
        render::Material::SlotHandles{},
        resource::MaterialData{},
        albedoSampler, normalSampler, registry));
    for (const resource::MaterialData& data : imported.materials) {
        // Slot handles are gathered in a loop over the same enum the shader
        // indexes its arrays with.
        render::Material::SlotHandles handles{};
        for (size_t slot = 0; slot < resource::kMaterialSlotCount; ++slot) {
            handles[slot] = resolveSlotTexture(imported, data,
                                               static_cast<resource::MaterialTextureSlot>(slot),
                                               assets);
        }
        materialsByIndex.emplace_back(&materials.create(
            std::move(handles), data, albedoSampler, normalSampler, registry));
    }

    glm::mat4 placement = glm::scale(glm::mat4(1.0f), glm::vec3(.1f, .1f, .1f));

    size_t totalVertices = 0;
    for (size_t i = 0; i < imported.primitives.size(); ++i) {
        const resource::GltfPrimitive& prim = imported.primitives[i];
        const glm::mat4 modelMatrix = placement * prim.world;

        totalVertices += prim.mesh.vertices().size();

        const std::string key = config_.modelPath + "#prim" + std::to_string(i);
        auto meshHandle = assets.loadMeshData(key, prim.mesh);

        render::Material* material =
            prim.materialIndex < imported.materials.size()
                ? materialsByIndex[prim.materialIndex + 1]
                : materialsByIndex[0];
        // aabbCenter is mesh-local; SceneObject derives one world center per
        // instance from the instance matrices at setInstances() time.
        auto object = std::make_shared<scene::SceneObject>(meshHandle, prim.aabbCenter, material, registry);
        std::vector<resource::InstanceData> instances(1);
        instances[0].model = modelMatrix;
        object->setInstances(queue, std::move(instances));
        scene_.addObject(std::move(object));
    }

    platform::LogLocator::get().write(platform::LogLevel::Info,
        "DemoScene: '" + config_.modelPath + "': " +
        std::to_string(imported.primitives.size()) + " primitives, " +
        std::to_string(imported.materials.size()) + " materials, " +
        std::to_string(totalVertices) + " vertices");
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
