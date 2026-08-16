#include "camera.hpp"

#include <algorithm>

Camera::Camera(float azimuth, float polar, double distance)
    : azimuth(azimuth), polar(polar), distance(distance) {}

void Camera::onMouseButton(platform::MouseButton button, platform::ButtonAction action, double cursorX, double cursorY) {
    if (button != platform::MouseButton::Left) return;

    if (action == platform::ButtonAction::Press) {
        leftPressed = true;
        lastX       = cursorX;
        lastY       = cursorY;
    } else if (action == platform::ButtonAction::Release) {
        leftPressed = false;
    }
}

void Camera::onCursorMove(double xPos, double yPos) {
    if (!leftPressed) return;

    double dx = xPos - lastX;
    double dy = yPos - lastY;
    lastX = xPos;
    lastY = yPos;

    azimuth -= static_cast<float>(dx) * sensitivity;
    polar   += static_cast<float>(dy) * sensitivity;

    clampPolar();
}

glm::mat4 Camera::viewMatrix() const {
    return glm::lookAt(position(), target_, glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::vec3 Camera::position() const {
    return {
        target_.x + distance * std::sin(polar) * std::sin(azimuth),
        target_.y + distance * std::cos(polar),
        target_.z + distance * std::sin(polar) * std::cos(azimuth)
    };
}

void Camera::moveHorizontal(float forward, float right,
                            float deltaTime, float speed) {
    // offset = (sin(polar)*sin(azimuth), cos(polar), sin(polar)*cos(azimuth))
    // The horizontal direction FROM camera TO target = normalize of
    // (-offset.x, 0, -offset.z) 
    // forwardXZ = (-sin(azimuth), 0, -cos(azimuth)) 
    glm::vec3 forwardXZ{-std::sin(azimuth), 0.0f, -std::cos(azimuth)};
    // right = cross(up, forward) -> rotated 90° CCW around Y
    glm::vec3 rightXZ{std::cos(azimuth), 0.0f, -std::sin(azimuth)};

    float step = speed * deltaTime;
    target_ += forwardXZ * forward * step;
    target_ += rightXZ  * right   * step;
}

void Camera::moveVertical(float direction, float deltaTime, float speed) {
    target_.y += direction * speed * deltaTime;
}

void Camera::clampPolar() {
    polar = std::clamp(polar, kPolarEpsilon, glm::pi<float>() - kPolarEpsilon);
}

void Camera::Zoom(double yOff) {
    distance -= yOff * 0.5f;
    distance = std::clamp(distance, kMinDistance, kMaxDistance);
}
