#include "scene/camera.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace scene {
constexpr float sensitivity = 0.001f;
constexpr float eps = std::numeric_limits<float>::min();
constexpr glm::vec3 cameraUp{0.0f, 1.0f, 0.0f};

Camera::Camera(float azimuth, float polar, float distance)
    : azimuth_(azimuth), polar_(polar), distance_(distance) {}

void Camera::orbit(float dAzimuth, float dPolar) {
    azimuth_ += dAzimuth *sensitivity;
    polar_   += dPolar * sensitivity;
    clampPolar();
}

void Camera::look(float dxPixels, float dyPixels) {
    const glm::vec3 anchor = position();
    azimuth_ -= dxPixels * sensitivity;
    polar_   -= dyPixels * sensitivity;
    clampPolar();

    glm::vec3 front;
    front.x = std::sin(polar_) * std::sin(azimuth_);
    front.y = std::cos(polar_);
    front.z = std::sin(polar_) * std::cos(azimuth_);
    front = glm::normalize(front);

    target_ = anchor - front * distance_;
}

glm::mat4 Camera::viewMatrix() const {
    return glm::lookAt(position(), target_, cameraUp);
}

glm::mat4 Camera::projectionMatrix(float aspect) const {
    if (projection_ == Projection::Orthographic) {
        const float halfHeight = 0.5f * orthoHeight_;
        const float halfWidth  = halfHeight * aspect;
        return glm::ortho(-halfWidth, halfWidth, -halfHeight, halfHeight,
                          nearPlane_, farPlane_);
    }
    return glm::perspective(glm::radians(fovDegrees_), aspect, nearPlane_, farPlane_);
}

glm::vec3 Camera::position() const {
    glm::vec3 front = {
        std::sin(polar_) * std::sin(azimuth_),
        std::cos(polar_),
        std::sin(polar_) * std::cos(azimuth_)
    };
    return target_ + distance_ * front;
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
    glm::vec3 moveDir = forwardXZ * forward + rightXZ * right;
    if(glm::length(moveDir) > eps) {
        moveDir = glm::normalize(moveDir);
    }
    target_ += moveDir * step;
}

void Camera::moveVertical(float direction, float deltaTime, float speed) {
    target_.y += direction * speed * deltaTime;
}

void Camera::pan(float dxPiexls, float dyPixels) {
    glm::vec3 rightXZ{std::cos(azimuth_), 0.0f, -std::sin(azimuth_)};
    float scale = static_cast<float>(distance_) * sensitivity;
    target_ += (rightXZ * dxPiexls + cameraUp * dyPixels) * scale;
}

void Camera::clampPolar() {
    polar_ = std::clamp(polar_, kPolarEpsilon, glm::pi<float>() - kPolarEpsilon);
}

void Camera::zoom(double yOffset) {
    if (projection_ == Projection::Orthographic) {
        // Multiplicative view-height zoom; scroll up (yOffset > 0) zooms in.
        orthoHeight_ = static_cast<float>(
            static_cast<double>(orthoHeight_) * std::exp(-yOffset * kOrthoZoomSpeed));
        orthoHeight_ = std::clamp(orthoHeight_, kMinOrthoHeight, kMaxOrthoHeight);
        return;
    }
    distance_ -= yOffset * 0.5f;
    distance_ = std::clamp(distance_, kMinDistance, kMaxDistance);
}

}  // namespace scene
