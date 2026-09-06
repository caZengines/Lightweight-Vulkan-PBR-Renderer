#pragma once
// ============================================================================
// Platform Abstraction Layer: Window
//
// The only place in the engine that talks to GLFW directly. All other layers
// interact with the window through this interface (events, size queries,
// Vulkan surface/extensions interop).
// ============================================================================

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "platform/input.hpp"   // MouseButton / ButtonAction

// Opaque forward declarations — the real definitions live only in window.cpp.
typedef struct GLFWwindow GLFWwindow;
typedef struct VkInstance_T* VkInstance;
typedef struct VkSurfaceKHR_T* VkSurfaceKHR;

namespace platform {

struct WindowConfig {
    uint32_t   width    = 1920;
    uint32_t   height   = 1080;
    const char* title   = "Vulkan";
    bool       resizable = true;
};

enum class CursorMode {
    Normal,
    Disabled
};

class Window {
public:
    explicit Window(const WindowConfig& config);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    void setCursorMode(CursorMode mode);

    void pollEvents();
    void waitEvents();
    bool shouldClose() const;

    uint32_t framebufferWidth()  const;
    uint32_t framebufferHeight() const;

    // Runtime title changes (e.g. camera status); title() tracks the latest.
    void setTitle(const std::string& title);
    [[nodiscard]] const std::string& title() const { return title_; }

    // --- RHI interop (Layer 1 only) ---
    // Instance extensions required by the window system (e.g. VK_KHR_surface
    // + platform surface extensions). Consumed by VulkanDevice.
    std::vector<const char*> requiredInstanceExtensions() const;
    // Creates the OS window surface; caller wraps it in a vk::raii::SurfaceKHR.
    VkSurfaceKHR createSurface(VkInstance instance) const;

    // --- Event hooks ---
    std::function<void(uint32_t /*width*/, uint32_t /*height*/)>             onFramebufferResize;

    // Scroll wheel delta accumulated since the last call (consumed by Input).
    double consumeScrollDelta() const;

private:
    friend class Input;   // Input::poll needs the native handle
    GLFWwindow* nativeHandle() const { return window_; }

    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
    static void scrollCallback(GLFWwindow* window, double xOffset, double yOffset);

    GLFWwindow* window_      = nullptr;
    std::string title_;
    mutable double scrollAccum_ = 0.0;
};

} // namespace platform
