#include "c_engine.hpp"
#include "camera.hpp"

#include <GLFW/glfw3.h>
#include <algorithm>

Camera::Camera(float azimuth, float polar, double distance)
    : azimuth(azimuth), polar(polar), distance(distance) {}

void Camera::onMouseButton(int button, int action, double cursorX, double cursorY) {
    if (button != GLFW_MOUSE_BUTTON_LEFT) return;

    if (action == GLFW_PRESS) {
        leftPressed = true;
        lastX       = cursorX;
        lastY       = cursorY;
    } else if (action == GLFW_RELEASE) {
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
    double delta_dist = distance - yOff * 0.5f;
    if(std::abs(delta_dist) < 0.01f) {
        distance = 0.01f;
    }
    else {
        distance = delta_dist;
    }
}


void CEngine::mouseButtonCallBack(GLFWwindow* window, int button, int action, int /*mods*/) {
    auto* app = static_cast<CEngine*>(glfwGetWindowUserPointer(window));
    double x = 0.0, y = 0.0;
    glfwGetCursorPos(window, &x, &y);
    app->camera.onMouseButton(button, action, x, y);
}

void CEngine::cursorPosCallBack(GLFWwindow* window, double xPos, double yPos) {
    auto* app = static_cast<CEngine*>(glfwGetWindowUserPointer(window));
    app->camera.onCursorMove(xPos, yPos);
}

void CEngine::scrollCallBack(GLFWwindow* window, double xOffset, double yOffset) {
    auto* app = static_cast<CEngine*>(glfwGetWindowUserPointer(window));
    app->camera.Zoom(yOffset);
}