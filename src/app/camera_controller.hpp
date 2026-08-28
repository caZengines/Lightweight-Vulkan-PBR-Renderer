#pragma once

#include "platform/input.hpp"

namespace scene {
class Camera;
}  // namespace scene

namespace app {

// Translates raw platform input into scene::Camera calls (Layer 5):
// left-drag orbit, WASD pan, Space/Shift vertical move, wheel zoom. Owns the
// drag state machine so the camera itself stays pure math.
class CameraController {
public:
    explicit CameraController(scene::Camera& camera) : camera_(camera) {}

    // Window event hooks.
    void onMouseButton(platform::MouseButton button, platform::ButtonAction action,
                       double cursorX, double cursorY);
    void onCursorMove(double xPos, double yPos);

    // Per-frame: keyboard pan + scroll zoom.
    void update(const platform::Input& input, float deltaTime);

private:
    scene::Camera& camera_;

    // Left-drag orbit state.
    bool   dragging_ = false;
    double lastX_ = 0.0;
    double lastY_ = 0.0;
};

}  // namespace app
