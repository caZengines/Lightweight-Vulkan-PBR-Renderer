#pragma once

#include <memory>
#include <vector>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "render/frame_uniforms.hpp"

class Material;
class Sampler;

namespace scene {
class Scene;
}  // namespace scene

namespace resource {
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

    [[nodiscard]] const std::vector<std::shared_ptr<Material>>& materials() const { return materials_; }
    [[nodiscard]] const render::FrameParams& frameParams() const { return frameParams_; }

private:
    const Config& config_;
    scene::Scene& scene_;

    std::vector<std::shared_ptr<Material>> materials_;
    render::FrameParams frameParams_{};
};

}  // namespace app
