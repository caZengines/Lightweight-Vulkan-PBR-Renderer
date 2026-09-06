#include "platform/window.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdexcept>

namespace platform {

Window::Window(const WindowConfig& config) {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, config.resizable ? GLFW_TRUE : GLFW_FALSE);

    window_ = glfwCreateWindow(static_cast<int>(config.width),
                               static_cast<int>(config.height),
                               config.title, nullptr, nullptr);
    if (!window_) {
        throw std::runtime_error("failed to create GLFW window");
    }
    title_ = config.title;

    glfwSetWindowUserPointer(window_, this);
    glfwSetFramebufferSizeCallback(window_, framebufferResizeCallback);
    glfwSetScrollCallback(window_, scrollCallback);
}

Window::~Window() {
    if (window_) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }
    glfwTerminate();
}

void Window::pollEvents() {
    glfwPollEvents();
}

void Window::waitEvents() {
    glfwWaitEvents();
}

bool Window::shouldClose() const {
    return glfwWindowShouldClose(window_) != 0;
}

void Window::setCursorMode(CursorMode mode) {
    int glfwMode = GLFW_CURSOR_NORMAL;
    switch (mode) {
        case CursorMode::Normal:  glfwMode = GLFW_CURSOR_NORMAL; break;
        case CursorMode::Disabled:glfwMode = GLFW_CURSOR_DISABLED; break;
    }
    glfwSetInputMode(window_, GLFW_CURSOR, glfwMode);
}

uint32_t Window::framebufferWidth() const {
    int width = 0;
    glfwGetFramebufferSize(window_, &width, nullptr);
    return static_cast<uint32_t>(width);
}

uint32_t Window::framebufferHeight() const {
    int height = 0;
    glfwGetFramebufferSize(window_, nullptr, &height);
    return static_cast<uint32_t>(height);
}

void Window::setTitle(const std::string& title) {
    title_ = title;
    glfwSetWindowTitle(window_, title.c_str());
}

std::vector<const char*> Window::requiredInstanceExtensions() const {
    uint32_t count = 0;
    const char** extensions = glfwGetRequiredInstanceExtensions(&count);
    return std::vector<const char*>(extensions, extensions + count);
}

VkSurfaceKHR Window::createSurface(VkInstance instance) const {
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (glfwCreateWindowSurface(instance, window_, nullptr, &surface) != 0) {
        throw std::runtime_error("failed to create window surface");
    }
    return surface;
}

double Window::consumeScrollDelta() const {
    double delta = scrollAccum_;
    scrollAccum_ = 0.0;
    return delta;
}

// ----------------------------------------------------------------------------
// Static GLFW trampolines — GLFW types never leave this translation unit.
// ----------------------------------------------------------------------------

void Window::framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self && self->onFramebufferResize) {
        self->onFramebufferResize(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
    }
}

void Window::scrollCallback(GLFWwindow* window, double /*xOffset*/, double yOffset) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self) {
        self->scrollAccum_ += yOffset;
    }
}

} // namespace platform
