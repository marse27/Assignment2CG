// cpp
#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <GLFW/glfw3.h>

class FreeCamera {
public:
    glm::vec3 position;
    glm::vec3 front;
    glm::vec3 up;
    glm::vec3 right;
    glm::vec3 worldUp;

    float yaw;
    float pitch;
    float movementSpeed;
    float fov; // degrees
    float mouseSensitivity;

    static FreeCamera* current; // used for static callbacks

    FreeCamera(const glm::vec3& startPos = glm::vec3(0.0f, 2.0f, 6.0f),
               const glm::vec3& upVec = glm::vec3(0.0f, 1.0f, 0.0f),
               float startYaw = -90.0f, float startPitch = 0.0f);

    // Call each frame. Polls WASD + vertical keys from the provided GLFW window.
    void update(GLFWwindow* window, float deltaTime);

    glm::mat4 getViewMatrix() const;

    // Register which camera receives scroll/mouse events
    static void setCurrent(FreeCamera* cam);

    // GLFW callbacks (register with glfwSetScrollCallback, glfwSetMouseButtonCallback, glfwSetCursorPosCallback)
    static void scrollCallback(GLFWwindow* /*window*/, double /*xoffset*/, double yoffset);
    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos);

private:
    void updateVectors();

    // mouse look state
    bool rightMouseDown = false;
    bool firstMouse = true;
    double lastX = 0.0, lastY = 0.0;
};
