#include "app/camera_controller.hpp"

#include "scene/camera.hpp"

namespace app {

void CameraController::onMouseButton(platform::MouseButton button, platform::ButtonAction action,
                                     double cursorX, double cursorY) {
    if (button != platform::MouseButton::Left) return;

    if (action == platform::ButtonAction::Press) {
        dragging_ = true;
        lastX_    = cursorX;
        lastY_    = cursorY;
    } else if (action == platform::ButtonAction::Release) {
        dragging_ = false;
    }
}

void CameraController::onCursorMove(double xPos, double yPos) {
    if (!dragging_) return;

    const float dx = static_cast<float>(xPos - lastX_);
    const float dy = static_cast<float>(yPos - lastY_);
    lastX_ = xPos;
    lastY_ = yPos;

    constexpr float sensitivity = 0.001f;
    camera_.orbit(-dx * sensitivity, dy * sensitivity);
}

void CameraController::update(const platform::Input& input, float deltaTime) {
    float forward = 0.0f, right = 0.0f;
    if (input.isKeyDown(platform::Key::W))         forward += 1.0f;
    if (input.isKeyDown(platform::Key::A))         right   -= 1.0f; 
    if (input.isKeyDown(platform::Key::S))         forward -= 1.0f;
    if (input.isKeyDown(platform::Key::D))         right   += 1.0f; 
    camera_.moveHorizontal(forward, right, deltaTime);
    if (input.isKeyDown(platform::Key::Space))     camera_.moveVertical(1.0f, deltaTime);
    if (input.isKeyDown(platform::Key::LeftShift)) camera_.moveVertical(-1.0f, deltaTime);

    const double scroll = input.scrollDelta();
    if (scroll != 0.0) camera_.zoom(scroll);
}

}  // namespace app
