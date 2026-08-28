#pragma once

#include <functional>

namespace platform {
class Window;
class Input;
}  // namespace platform

namespace app {

// event polling, delta-time measurement,
// and the update → render step sequence.
//
// Fixed-timestep: once simulation logic exists, accumulate dt here and
// run update() in fixed slices; the interactive camera keeps consuming the
// raw frame delta (input responsiveness must not wait on simulation steps).
class GameLoop {
public:
    using UpdateFn = std::function<void(float)>;  // dt in seconds
    using FrameFn  = std::function<bool()>;       // false = frame skipped

    GameLoop(platform::Window& window, platform::Input& input)
        : window_(window), input_(input) {}

    void setUpdate(UpdateFn fn) { onUpdate_ = std::move(fn); }
    void setFrame(FrameFn fn)   { onFrame_  = std::move(fn); }

    void run();

private:
    platform::Window& window_;
    platform::Input&  input_;
    UpdateFn onUpdate_;
    FrameFn  onFrame_;
};

}  // namespace app
