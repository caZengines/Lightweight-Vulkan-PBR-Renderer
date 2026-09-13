#pragma once

#include <memory>
#include <vector>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "render/frame_uniforms.hpp"
#include "resource/asset_handle.hpp"
#include "resource/gltf_importer.hpp"


class Sampler;

namespace scene {
class Scene;
}  // namespace scene

namespace resource {
class Material;
class AssetLibrary;
class ResourceRegistry;
class UploadQueue;
}  // namespace resource

namespace app {

struct Config;

// Demo content (from the old composition root's createMaterials/initScene):
// three materials, one mars, 1000 randomly placed rocks — plus the demo
// light/projection constants that used to be literals inside the renderer's
// uniform fill. Materials are owned here; the composition root allocates one
// Set-1 descriptor set per material after build().
class DemoScene {
public:
    DemoScene(const Config& config, scene::Scene& scene);

    // Loads assets, creates the materials and the scene objects. The resource
    // services and samplers are composition-root pieces shared by all content.
    void build(const Sampler& albedoSampler, const Sampler& normalSampler,
               resource::AssetLibrary& assets, resource::ResourceRegistry& registry,
               resource::UploadQueue& queue);

    [[nodiscard]] const std::vector<std::shared_ptr<resource::Material>>& materials() const { return materials_; }
    [[nodiscard]] const render::FrameParams& frameParams() const { return frameParams_; }

private:
    // glTF scene content: one SceneObject per primitive, one Material per
    // glTF material (materials_[0] is the fallback for primitives without
    // one). Textures resolve through the asset library, so shared textures
    // upload once.
    void buildglTFdemo(const Sampler& albedoSampler, const Sampler& normalSampler,
                  resource::AssetLibrary& assets, resource::ResourceRegistry& registry,
                  resource::UploadQueue& queue);

    // Resolves one material texture slot to an asset handle; empty handle
    // when the slot has no usable texture (Material falls back to the
    // built-in default textures).
    [[nodiscard]] resource::AssetHandle resolveSlotTexture(
        const resource::GltfScene& imported,
        const resource::MaterialData& material,
        resource::MaterialTextureSlot slot,
        resource::AssetLibrary& assets) const;

    const Config& config_;
    scene::Scene& scene_;

    std::vector<std::shared_ptr<resource::Material>> materials_;
    render::FrameParams frameParams_{};
};

}  // namespace app
