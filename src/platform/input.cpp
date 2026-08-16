#include "platform/input.hpp"
#include "platform/window.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
namespace platform {

namespace {

// GLFW key codes for our Key enum, index-aligned.
constexpr int kGlfwKeys[static_cast<uint32_t>(Key::Count)] = {
    GLFW_KEY_W,
    GLFW_KEY_A,
    GLFW_KEY_S,
    GLFW_KEY_D,
    GLFW_KEY_SPACE,
    GLFW_KEY_LEFT_SHIFT,
};

// GLFW mouse button codes: LEFT=0, RIGHT=1, MIDDLE=2.
constexpr int kGlfwMouseButtons[3] = {
    GLFW_MOUSE_BUTTON_LEFT,
    GLFW_MOUSE_BUTTON_RIGHT,
    GLFW_MOUSE_BUTTON_MIDDLE,
};

} // namespace

void Input::poll(const Window& window) {
    GLFWwindow* native = window.nativeHandle();

    for (uint32_t i = 0; i < static_cast<uint32_t>(Key::Count); ++i) {
        keys_[i] = (glfwGetKey(native, kGlfwKeys[i]) == GLFW_PRESS) ? 1 : 0;
    }
    for (uint32_t i = 0; i < 3; ++i) {
        buttons_[i] = (glfwGetMouseButton(native, kGlfwMouseButtons[i]) == GLFW_PRESS) ? 1 : 0;
    }

    double x = 0.0, y = 0.0;
    glfwGetCursorPos(native, &x, &y);
    if (firstPoll_) {
        lastX_ = x;
        lastY_ = y;
        firstPoll_ = false;
    }
    deltaX_ = x - lastX_;
    deltaY_ = y - lastY_;
    lastX_  = x;
    lastY_  = y;

    scroll_ = window.consumeScrollDelta();
}

bool Input::isKeyDown(Key key) const {
    const uint32_t index = static_cast<uint32_t>(key);
    return index < static_cast<uint32_t>(Key::Count) && keys_[index] != 0;
}

bool Input::isMouseDown(MouseButton button) const {
    const uint32_t index = static_cast<uint32_t>(button);
    return index < 3 && buttons_[index] != 0;
}

} // namespace platform
