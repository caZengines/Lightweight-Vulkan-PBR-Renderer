#pragma once
// ============================================================================
// Platform Abstraction Layer: Input
//
// Poll-based keyboard / mouse / scroll state. GLFW key & button constants are
// mapped to engine enums here; no GLFW type appears in the interface.
// ============================================================================

#include <array>
#include <cstdint>

namespace platform {

enum class Key : uint32_t {
    W = 0,
    A,
    S,
    D,
    Q,
    E,
    Digit1, Digit2, Digit3,
    Digit4, Digit5, Digit6,
    Digit7, Digit8, Digit9,
    KP5,
    Space,
    LeftShift,
    LeftCtrl,
    LeftAlt,
    Tab,
    Count
};

enum class MouseButton : uint8_t { Left = 0, Right = 1, Middle = 2 };

class Window;

class Input {
public:
    Input() = default;

    void attach(Window& window);
    void detach(Window& window);

    // Call once per frame after Window::pollEvents().
    void poll(const Window& window);

    bool isDown(Key key) const;

    bool isDown(MouseButton button) const;
    //in frame Events
    bool wasKeyPressed(Key key) const;
    bool wasMouseButtonPressed(MouseButton button) const;

    // Cursor movement since the last poll (pixels, y-up screen space).
    double cursorDeltaX() const { return deltaX_; }
    double cursorDeltaY() const { return deltaY_; }

    // Scroll wheel delta accumulated since the last poll.
    double scrollDelta() const { return scroll_; }

    // glfwEvents Callback
    void markKeyPressed(int glfwKey);
    void markMousePressed(int glfwButton);
    // when frame begin cleared
    void clearFrameEvents();
    // used for window lost focus
    void releaseAll();

private:
    std::array<uint32_t, static_cast<uint32_t>(Key::Count)> keys_{};
    std::array<uint32_t, 3> buttons_{};
    std::array<bool, static_cast<uint32_t>(Key::Count)> pressedThisFrame_{};
    std::array<bool, 3> pressedButtonsThisFrame_{};
    double   lastX_ = 0.0, lastY_ = 0.0;
    double   deltaX_ = 0.0, deltaY_ = 0.0;
    double   scroll_ = 0.0;
    bool     firstPoll_ = true;
};

} // namespace platform
