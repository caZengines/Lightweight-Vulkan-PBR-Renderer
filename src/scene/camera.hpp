#pragma once

#include <cstdint>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace scene {

enum class Projection : uint8_t {
    Perspective,
    Orthographic,
};

// Orbit camera: pure math. Input plumbing (mouse drag, keys, scroll) belongs
// to the app layer, which drives this class through the mutators below.
// Projection params travel with the camera (perspective fov or orthographic
// view height + near/far), so cameras of different types coexist and each
// frame renders through exactly one of them.
class Camera {
public:
    Camera(float azimuth   = glm::radians(45.0f),
           float polar     = glm::radians(45.0f),
           float distance = 32.0f);

    // Rotate the orbit; polar is clamped away from the singularities.
    void orbit(float dAzimuth, float dPolar);

    // First-person mouse look (roam mode): pixel deltas rotate the view
    // heading (azimuth = yaw, polar = pitch, clamped away from the poles).
    // Translation is separate: moveHorizontal/moveVertical, scaled by dt.
    void look(float dxPixels, float dyPixels);

    // --- panning (editor-style) ---
    // move horizontally (forward = target direction flattened to XZ plane,
    // right = forward rotated 90° CCW around Y)
    void moveHorizontal(float forward, float right, float deltaTime, float speed = 8.0f);
    // move vertically in world space (+1 = up, -1 = down)
    void moveVertical(float direction, float deltaTime, float speed = 8.0f);

    void pan(float dxPiexls, float dyPixels);

    // scroll-wheel driven: dolly (perspective) or view-height scaling (ortho)
    void zoom(double yOffset);

    [[nodiscard]] glm::mat4 viewMatrix() const;
    // aspect = render-target width / height
    [[nodiscard]] glm::mat4 projectionMatrix(float aspect) const;

    [[nodiscard]] glm::vec3 position() const;
    [[nodiscard]] glm::vec3 target()   const { return target_; }

    // In-place projection switch; pose is preserved.
    void setProjection(Projection projection) { projection_ = projection; }
    [[nodiscard]] Projection projection() const { return projection_; }

private:
    float  azimuth_;
    float  polar_;
    float  distance_;

    static constexpr float kMinDistance = 0.01;
    static constexpr float kMaxDistance = 100.0;

    glm::vec3 target_{0.0f, 0.0f, 0.0f};

    static constexpr float kPolarEpsilon = 0.001f;

    // Projection state; defaults are the literals FrameParams used to carry,
    // so the first frame stays identical.
    Projection projection_ = Projection::Perspective;
    float fovDegrees_  = 45.0f;   // perspective vertical fov
    float orthoHeight_ = 10.0f;   // orthographic view height (world units)
    float nearPlane_   = 0.1f;
    float farPlane_    = 100.0f;

    static constexpr float  kMinOrthoHeight  = 0.1f;
    static constexpr float  kMaxOrthoHeight  = 200.0f;
    static constexpr double kOrthoZoomSpeed  = 0.15;

    void clampPolar();
};

}  // namespace scene
