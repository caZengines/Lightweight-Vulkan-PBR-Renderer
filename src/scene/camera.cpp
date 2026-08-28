#include "scene/camera.hpp"

#include <algorithm>

namespace scene {

Camera::Camera(float azimuth, float polar, double distance)
    : azimuth_(azimuth), polar_(polar), distance_(distance) {}

void Camera::orbit(float dAzimuth, float dPolar) {
    azimuth_ += dAzimuth;
    polar_   += dPolar;
    clampPolar();
}

glm::mat4 Camera::viewMatrix() const {
    return glm::lookAt(position(), target_, glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::vec3 Camera::position() const {
    return {
        target_.x + distance_ * std::sin(polar_) * std::sin(azimuth_),
        target_.y + distance_ * std::cos(polar_),
        target_.z + distance_ * std::sin(polar_) * std::cos(azimuth_)
    };
}

void Camera::moveHorizontal(float forward, float right,
                            float deltaTime, float speed) {
    // offset = (sin(polar)*sin(azimuth), cos(polar), sin(polar)*cos(azimuth))
    // The horizontal direction FROM camera TO target = normalize of (-offset.x, 0, -offset.z)
    // forwardXZ = (-sin(azimuth), 0, -cos(azimuth))
    glm::vec3 forwardXZ{-std::sin(azimuth_), 0.0f, -std::cos(azimuth_)};
    // right = cross(up, forward) -> rotated 90° CCW around Y
    glm::vec3 rightXZ{std::cos(azimuth_), 0.0f, -std::sin(azimuth_)};

    float step = speed * deltaTime;
    target_ += forwardXZ * forward * step;
    target_ += rightXZ  * right   * step;
}

void Camera::moveVertical(float direction, float deltaTime, float speed) {
    target_.y += direction * speed * deltaTime;
}

void Camera::clampPolar() {
    polar_ = std::clamp(polar_, kPolarEpsilon, glm::pi<float>() - kPolarEpsilon);
}

void Camera::zoom(double yOffset) {
    distance_ -= yOffset * 0.5f;
    distance_ = std::clamp(distance_, kMinDistance, kMaxDistance);
}

}  // namespace scene
