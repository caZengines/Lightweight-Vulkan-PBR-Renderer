#include "rhi/surface.hpp"

#include "platform/window.hpp"

namespace rhi {

Surface::Surface(const vk::raii::Instance& instance, platform::Window& window)
    : surface_(instance, window.createSurface(*instance)) {}

}  // namespace rhi
