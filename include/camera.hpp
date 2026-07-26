#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

class Camera {
public:
    Camera(float azimuth  = glm::radians(45.0f),
           float polar    = glm::radians(45.0f),
           double = 32.0f);

    void onMouseButton(int button, int action, double cursorX, double cursorY);
    void onCursorMove(double xPos, double yPos);

    glm::mat4 viewMatrix() const;
    glm::vec3 position() const;
    glm::vec3 target()   const { return target_; }

    // --- WASD / Space / LShift movement (editor-style pan) ---
    // move horizontally (W/S = forward/back, A/D = left/right)
    // "forward" = direction from camera to target, flattened to XZ plane
    void moveHorizontal(float forward, float right, float deltaTime, float speed = 8.0f);
    // move vertically in world space (+1 = up / Space, -1 = down / LShift)
    void moveVertical(float direction, float deltaTime, float speed = 8.0f);

    void Zoom(double yOff);

    float getAzimuth()  const { return azimuth; }
    float getPolar()    const { return polar; }
    float getDistance() const { return distance; }

    void setAzimuth(float a)  { azimuth  = a; }
    void setPolar(float p)    { polar    = p; clampPolar(); }
    void setDistance(float d) { distance = d; }
    void setSensitivity(float s) { sensitivity = s; }

private:
    float azimuth;
    float polar;
    double distance;

    glm::vec3 target_{0.0f, 0.0f, 0.0f};

    //mouse 
    bool   leftPressed = false;
    double lastX       = 0.0;
    double lastY       = 0.0;
    float  sensitivity = 0.001f;

    static constexpr float kPolarEpsilon = 0.001f;

    void clampPolar();
};
