#pragma once

#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

namespace platform {
class Window;
}  // namespace platform

namespace rhi {

// Window-system surface, split out of the old god-config Context. The only
// place besides platform/ that knows how to turn a platform window into a
// VkSurfaceKHR — via Window::createSurface, keeping GLFW confined below.
class Surface final {
public:
    explicit Surface(const vk::raii::Instance& instance, platform::Window& window);

    [[nodiscard]] const vk::raii::SurfaceKHR& handle() const noexcept { return surface_; }

private:
    vk::raii::SurfaceKHR surface_ = nullptr;
};

}  // namespace rhi
