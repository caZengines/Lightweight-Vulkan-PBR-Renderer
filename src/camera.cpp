#include "c_engine.hpp"
#include "camera.hpp"

#include <algorithm>

Camera::Camera(float azimuth, float polar, float distance)
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
    return glm::lookAt(position(), target(), glm::vec3(0.0f, 0.0f, 1.0f));
}

glm::vec3 Camera::position() const {
    return {
        distance * std::sin(polar) * std::sin(azimuth),
        distance * std::sin(polar) * std::cos(azimuth),
        distance * std::cos(polar),
        
    };
}

void Camera::clampPolar() {
    polar = std::clamp(polar, kPolarEpsilon, glm::pi<float>() - kPolarEpsilon);
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
