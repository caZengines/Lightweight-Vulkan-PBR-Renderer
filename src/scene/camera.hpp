#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace scene {

// Orbit camera: pure math. Input plumbing (mouse drag, keys, scroll) belongs
// to the app layer, which drives this class through the mutators below.
class Camera {
public:
    Camera(float azimuth   = glm::radians(45.0f),
           float polar     = glm::radians(45.0f),
           double distance = 32.0f);

    // Rotate the orbit; polar is clamped away from the singularities.
    void orbit(float dAzimuth, float dPolar);

    // --- panning (editor-style) ---
    // move horizontally (forward = target direction flattened to XZ plane,
    // right = forward rotated 90° CCW around Y)
    void moveHorizontal(float forward, float right, float deltaTime, float speed = 8.0f);
    // move vertically in world space (+1 = up, -1 = down)
    void moveVertical(float direction, float deltaTime, float speed = 8.0f);

    // dolly toward/away from the target (scroll-wheel driven)
    void zoom(double yOffset);

    [[nodiscard]] glm::mat4 viewMatrix() const;
    [[nodiscard]] glm::vec3 position() const;
    [[nodiscard]] glm::vec3 target()   const { return target_; }

private:
    float  azimuth_;
    float  polar_;
    double distance_;

    static constexpr double kMinDistance = 0.01;
    static constexpr double kMaxDistance = 100.0;

    glm::vec3 target_{0.0f, 0.0f, 0.0f};

    static constexpr float kPolarEpsilon = 0.001f;

    void clampPolar();
};

}  // namespace scene
