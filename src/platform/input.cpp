#include "platform/input.hpp"
#include "platform/window.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
namespace platform {

namespace {

// GLFW key codes for our Key enum, index-aligned.
constexpr std::array<int, static_cast<uint32_t>(Key::Count)> kGlfwKeys = {
    GLFW_KEY_W,
    GLFW_KEY_A,
    GLFW_KEY_S,
    GLFW_KEY_D,
    GLFW_KEY_Q,
    GLFW_KEY_E,
    GLFW_KEY_1,
    GLFW_KEY_2,
    GLFW_KEY_3,
    GLFW_KEY_4,
    GLFW_KEY_5,
    GLFW_KEY_6,
    GLFW_KEY_7,
    GLFW_KEY_8,
    GLFW_KEY_9,
    GLFW_KEY_KP_5,
    GLFW_KEY_SPACE,
    GLFW_KEY_LEFT_SHIFT,
    GLFW_KEY_LEFT_CONTROL,
    GLFW_KEY_LEFT_ALT,
    GLFW_KEY_TAB
};

// GLFW mouse button codes: LEFT=0, RIGHT=1, MIDDLE=2.
constexpr std::array<int, static_cast<uint32_t>(MouseButton::Middle) + 1> kGlfwMouseButtons = {
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

bool Input::isDown(Key key) const {
    const uint32_t index = static_cast<uint32_t>(key);
    return index < static_cast<uint32_t>(Key::Count) && keys_[index] != 0;
}

bool Input::isDown(MouseButton button) const {
    const uint32_t index = static_cast<uint32_t>(button);
    return index < 3 && buttons_[index] != 0;
}

} // namespace platform
