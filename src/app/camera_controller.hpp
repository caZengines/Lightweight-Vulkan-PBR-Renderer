#pragma once

#include <string>

#include "app/action_context.hpp"

namespace scene {
class CameraManager;
}  // namespace scene

namespace platform {
class Window;
}  // namespace platform

namespace app {

// Maps semantic actions from ActionContext onto the active scene::Camera
// Navigation mode is derived per frame from the action context — no
// persistent mode state here. Mirrors the active camera state into the
// window title.
class CameraController {
public:
    explicit CameraController(scene::CameraManager& cameras) : cameras_(cameras) {}

    // Injected from App::initWindow (the window is created after this member).
    void setWindow(platform::Window& window);

    // Per-frame: camera hotkeys + camera navigation + scroll zoom.
    void update(const ActionContext& actions, const platform::Input& input, float deltaTime);

private:
    void updateTitle();

    scene::CameraManager& cameras_;

    platform::Window*     window_ = nullptr;   // optional: title feedback only

    // Captured from the window on the first title refresh, before any
    // camera suffix is appended.
    std::string baseTitle_;
};

}  // namespace app
