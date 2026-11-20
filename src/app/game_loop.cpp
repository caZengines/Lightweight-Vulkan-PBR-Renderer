#include "app/game_loop.hpp"

#include "platform/input.hpp"
#include "platform/log.hpp"
#include "platform/window.hpp"

#include <GLFW/glfw3.h>
#include <chrono>
#include <string>

namespace app {

void GameLoop::run() {
    using clock = std::chrono::high_resolution_clock;

    auto lastTime = clock::now();
    while (!window_.shouldClose()) {
        window_.pollEvents();
        input_.poll(window_);

        const auto now = clock::now();
        const float deltaTime = std::chrono::duration<float>(now - lastTime).count();
        lastTime = now;

        if (onUpdate_) onUpdate_(deltaTime);
        // Clear only after the update consumed this frame's events: events
        // dispatched later by Window::waitEvents (swapchain rebuild while
        // minimized) must survive until the next update.
        input_.clearFrameEvents();
        if (onFrame_ && !onFrame_()) continue;  // frame skipped (swapchain rebuild)
    }
    platform::LogLocator::get().write(platform::LogLevel::Info, "App main loop exited, WindowShouldClose = " + std::to_string(window_.shouldClose()));
}

}  // namespace app
