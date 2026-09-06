#include "scene/camera_manager.hpp"

#include <cassert>
#include <string>

namespace scene {

uint32_t CameraManager::add(const Camera& camera) {
    std::string name = "Camera_" + std::to_string(nextIndex_);
    CameraEntry entry(camera, name);
    cameras_.emplace_back(entry);
    ++nextIndex_;
    return static_cast<uint32_t>(cameras_.size() - 1);
}

bool CameraManager::removeActive() {
    if(count() <= 1) return false;

    cameras_.erase(cameras_.begin() + active_);
    active_ = std::min(active_, count()-1);
    return true;
}

void CameraManager::cycle() {
    active_ = (active_ + 1) % count();
}

void CameraManager::setActive(size_t index) {
    if(index >= 0 && index < count()) {
        active_ = index;
    }
}

Camera& CameraManager::active() {
    assert(active_ < count());
    return cameras_[active_].camera;
}

const Camera& CameraManager::active() const {
    assert(active_ < count());
    return cameras_[active_].camera;
}

}  // namespace scene
