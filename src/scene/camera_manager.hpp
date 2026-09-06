#pragma once

#include <cstdint>
#include <vector>
#include <string>

#include "scene/camera.hpp"

namespace scene {

// Owns every camera and tracks which one renders. the app layer
// drives it (hotkeys), the renderer reads active() once per frame. The deque
// keeps references stable across add(), so nothing else needs re-wiring when
// a camera is created at runtime.
class CameraManager {
public:
    CameraManager() = default;   // starts with one default camera

    // Appends a copy and returns the new camera's index; does not switch.
    uint32_t add(const Camera& camera);

    bool removeActive();

    // Active index += 1, wrapping around.
    void cycle();
    void setActive(size_t index); 

    [[nodiscard]] Camera&       active();
    [[nodiscard]] const Camera& active() const;
    [[nodiscard]] uint32_t activeIndex() const { return active_; }
    [[nodiscard]] uint32_t count() const { return static_cast<uint32_t>(cameras_.size()); }

private:
    struct CameraEntry { scene::Camera camera; std::string name; };
    std::vector<CameraEntry> cameras_{CameraEntry(Camera(), "Camera_0")};
    uint32_t nextIndex_ = 1;
    uint32_t active_ = 0;
};

}  // namespace scene
