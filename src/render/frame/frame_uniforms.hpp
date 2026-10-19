#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

namespace render {

// Per-frame shader-facing data; the single authoritative home of the UBO layout.
struct Light {
    alignas(16) glm::vec4 pos = {0.0, 12.0f, 0.0f, 1.0f};
    alignas(16) glm::vec4 color = glm::vec4(1.0f);
    alignas(16) float intensity = 300;
};

// Layout is shared with shaders/raster/shader.slang — keep member order and
// alignment in sync with the shader block.
struct UniformBufferObject {
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
    alignas(16) glm::vec4 camPos;
    alignas(16) Light     light;
};

static_assert(sizeof(UniformBufferObject) == 192, "UBO layout drifted from the slang shader");

// CPU-side per-frame parameters supplied by the content layer (app::DemoScene)
// and consumed by Renderer::fillUniformBuffer.
struct FrameParams {
    Light light{};
};

}  // namespace render
