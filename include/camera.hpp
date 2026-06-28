#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>

#include <cmath>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

class Camera {
public:
    Camera(float azimuth  = glm::radians(45.0f),
           float polar    = glm::radians(45.0f),
           float distance = 4.0f);

    void onMouseButton(int button, int action, double cursorX, double cursorY);
    void onCursorMove(double xPos, double yPos);

    glm::mat4 viewMatrix() const; 
    glm::vec3 position() const;  
    glm::vec3 target()   const { return {0.0f, 0.0f, 0.0f}; }

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
    float distance;

    //mouse 
    bool   leftPressed = false;
    double lastX       = 0.0;
    double lastY       = 0.0;
    float  sensitivity = 0.001f;

    static constexpr float kPolarEpsilon = 0.001f;

    void clampPolar();
};
