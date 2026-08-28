#include "app/game_loop.hpp"

#include "platform/input.hpp"
#include "platform/window.hpp"

#include <chrono>

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
        if (onFrame_ && !onFrame_()) continue;  // frame skipped (swapchain rebuild)
    }
}

}  // namespace app
