#pragma once
// ============================================================================
// Platform Abstraction Layer: Input
//
// Poll-based keyboard / mouse / scroll state. GLFW key & button constants are
// mapped to engine enums here; no GLFW type appears in the interface.
// ============================================================================

#include <cstdint>

namespace platform {

enum class Key : uint32_t {
    W = 0,
    A,
    S,
    D,
    Space,
    LeftShift,
    Count
};

enum class MouseButton : uint8_t { Left = 0, Right = 1, Middle = 2 };
enum class ButtonAction : uint8_t { Press, Release };

class Window;

class Input {
public:
    Input() = default;

    // Call once per frame after Window::pollEvents().
    void poll(const Window& window);

    bool isKeyDown(Key key) const;

    bool isMouseDown(MouseButton button) const;

    // Cursor movement since the last poll (pixels, y-up screen space).
    double cursorDeltaX() const { return deltaX_; }
    double cursorDeltaY() const { return deltaY_; }

    // Scroll wheel delta accumulated since the last poll.
    double scrollDelta() const { return scroll_; }

private:
    uint32_t keys_[static_cast<uint32_t>(Key::Count)]    = {};
    uint32_t buttons_[3]                                 = {};
    double   lastX_ = 0.0, lastY_ = 0.0;
    double   deltaX_ = 0.0, deltaY_ = 0.0;
    double   scroll_ = 0.0;
    bool     firstPoll_ = true;
};

} // namespace platform
