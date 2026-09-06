#include "app/camera_controller.hpp"

#include "app/action_context.hpp"
#include "platform/window.hpp"
#include "scene/camera.hpp"
#include "scene/camera_manager.hpp"

namespace app {
namespace {

const char* projectionLabel(scene::Projection projection) {
    return projection == scene::Projection::Orthographic ? "Ortho" : "Persp";
}

}  // namespace

void CameraController::setWindow(platform::Window& window) {
    window_ = &window;
}

// Navigation mode is derived per frame from the ActionContext — no persistent
// mode state here. The action layer guarantees gesture mutual exclusion and
// gates the roam movement bindings, so plain if/else chains are sufficient.
void CameraController::update(const ActionContext& actions, const platform::Input& input, float deltaTime) {
    scene::Camera& camera = cameras_.active();

    if (actions.isActive(Action::toggleCameraControlMode)) {
        // Roam (hold Shift+RMB, Unreal-editor style): mouse steers the view
        // heading; WASD/QE translate, scaled by deltaTime.
        camera.look(input.cursorDeltaX(), input.cursorDeltaY());
        float forward = 0.0f, right = 0.0f;
        if (actions.isActive(Action::moveForward))  forward += 1.0f;
        if (actions.isActive(Action::moveBackward)) forward -= 1.0f;
        if (actions.isActive(Action::moveLeft))     right   -= 1.0f;
        if (actions.isActive(Action::moveRight))    right   += 1.0f;
        camera.moveHorizontal(forward, right, deltaTime);
        if (actions.isActive(Action::moveUp))   camera.moveVertical( 1.0f, deltaTime);
        if (actions.isActive(Action::moveDown)) camera.moveVertical(-1.0f, deltaTime);
    }
    else {
        if (actions.isActive(Action::cameraPan)) {
            camera.pan(input.cursorDeltaX(), input.cursorDeltaY());
        }
        else if (actions.isActive(Action::OrbitalRotation)) {
            camera.orbit(input.cursorDeltaX(), input.cursorDeltaY());
        }
        // Wheel zoom stays available during pan/orbit drags.
        const double scroll = input.scrollDelta();
        if (scroll != 0.0) camera.zoom(scroll);
    }

    // Cursor capture switches on gesture edges only.
    if (window_) {
        if (actions.onActivated(Action::toggleCameraControlMode)) {
            window_->setCursorMode(platform::CursorMode::Disabled);
        }
        if (actions.onDeactivated(Action::toggleCameraControlMode)) {
            window_->setCursorMode(platform::CursorMode::Normal);
        }
    }

    if (actions.onActivated(Action::toggleProjection)) {
        camera.setProjection(camera.projection() == scene::Projection::Perspective
                                 ? scene::Projection::Orthographic
                                 : scene::Projection::Perspective);
    }
}

void CameraController::updateTitle() {
    if (!window_) return;
    if (baseTitle_.empty()) baseTitle_ = window_->title();

    window_->setTitle(baseTitle_ + " | Cam "
                      + std::to_string(cameras_.activeIndex() + 1) + "/"
                      + std::to_string(cameras_.count())
                      + " [" + projectionLabel(cameras_.active().projection()) + "]");
}

}  // namespace app
