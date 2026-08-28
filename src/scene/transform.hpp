#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace scene {

// TRS transform: position + euler rotation (radians, applied Y→X→Z) + scale.
struct Transform {
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    glm::vec3 rotation{0.0f, 0.0f, 0.0f};  // euler angles in radians
    glm::vec3 scale{1.0f, 1.0f, 1.0f};

    // Compose model matrix in TRS order: T * R * S
    glm::mat4 toMatrix() const;
};

}  // namespace scene
