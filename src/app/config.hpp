#pragma once
#include "platform/utils.hpp"

#include <cstdint>
#include <string>

namespace app {

// Application configuration — composition-root prototype (Phase 5 will grow this).
// Every path is resolved to an absolute path against
// platform::PlatformUtils::assetRoot() at construction, so no "../" relative
// paths remain in the codebase (risk R5).
struct Config {
    // Absolute asset root (project root).
    std::string assetRoot;

    // Asset paths — stored relative, resolved to absolute in the constructor.
    std::string modelPath         = "models/scene.gltf";
    std::string rockPath          = "models/rock.obj";
    std::string planetPath        = "models/planet.obj";
    std::string texturePath       = "textures/container.png";
    std::string rockTexturePath   = "textures/rock.png";
    std::string marsTexturePath   = "textures/mars.png";
    std::string normalTexturePath = "textures/container_normal_OpenGL.png";
    std::string shaderPath        = "shaders/slang.spv";

    // Rendering quality knob fed into render::RenderSettings by the
    // composition root; clamped to device-supported sample counts there.
    uint32_t msaaSamples = 4;

    Config() {
        assetRoot         = platform::PlatformUtils::assetRoot();
        modelPath         = platform::PlatformUtils::assetPath(modelPath);
        rockPath          = platform::PlatformUtils::assetPath(rockPath);
        planetPath        = platform::PlatformUtils::assetPath(planetPath);
        texturePath       = platform::PlatformUtils::assetPath(texturePath);
        rockTexturePath   = platform::PlatformUtils::assetPath(rockTexturePath);
        marsTexturePath   = platform::PlatformUtils::assetPath(marsTexturePath);
        normalTexturePath = platform::PlatformUtils::assetPath(normalTexturePath);
        shaderPath        = platform::PlatformUtils::assetPath(shaderPath);
    }
};

}  // namespace app
