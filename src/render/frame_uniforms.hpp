#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

namespace render {

// Per-frame shader-facing data (was inlined in renderer.hpp; Phase 3 moved it
// out so the UBO layout has a single authoritative home).
struct Light {
    alignas(16) glm::vec4 pos;
    alignas(16) glm::vec4 color;
    alignas(16) float intensity;
};

// Layout is shared with shaders/slang.spv — keep member order and alignment
// in sync with the shader block.
struct UniformBufferObject {
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
    alignas(16) glm::vec4 camPos;
    alignas(16) Light     light;
};

static_assert(sizeof(UniformBufferObject) == 192, "UBO layout drifted from the slang shader");

// CPU-side per-frame parameters fed by the content layer (app::DemoScene) and
// consumed by Renderer::fillUniformBuffer — the light literal used to live in
// the renderer itself.
struct FrameParams {
    Light light{};
};

}  // namespace render
