// cpp
#include "free_camera.h"
#include <iostream>

FreeCamera *FreeCamera::current = nullptr;

FreeCamera::FreeCamera(const glm::vec3 &startPos, const glm::vec3 &upVec, float startYaw, float startPitch)
    : position(startPos),
      worldUp(upVec),
      yaw(startYaw),
      pitch(startPitch),
      movementSpeed(5.0f),
      fov(45.0f),
      mouseSensitivity(0.1f) {
    updateVectors();
}

void FreeCamera::update(GLFWwindow *window, float deltaTime) {
    float velocity = movementSpeed * deltaTime;

    // Movement: WASD
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        position += front * velocity;
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        position -= front * velocity;
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        position -= right * velocity;
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        position += right * velocity;
    }

    // Vertical movement: Space = up, C = down (and keep Left Shift as alternative down)
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
        position += worldUp * velocity;
    }
    if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS) {
        position -= worldUp * velocity;
    }
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
        position -= worldUp * velocity;
    }

    // Mouse look is handled by cursorPosCallback while rightMouseDown is true.
    // updateVectors() is called from the cursor callback when yaw/pitch change.
}

glm::mat4 FreeCamera::getViewMatrix() const {
    return glm::lookAt(position, position + front, up);
}

void FreeCamera::setCurrent(FreeCamera *cam) {
    current = cam;
}

void FreeCamera::scrollCallback(GLFWwindow * /*window*/, double /*xoffset*/, double yoffset) {
    if (!current) return;
    current->fov -= static_cast<float>(yoffset);
    if (current->fov < 20.0f) current->fov = 20.0f;
    if (current->fov > 90.0f) current->fov = 90.0f;
}

void FreeCamera::mouseButtonCallback(GLFWwindow *window, int button, int action, int /*mods*/) {
    if (!current) return;
    if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        if (action == GLFW_PRESS) {
            current->rightMouseDown = true;
            current->firstMouse = true; // reset so we don't jump on first movement
            // hide + capture cursor for look-around
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        } else if (action == GLFW_RELEASE) {
            current->rightMouseDown = false;
            // restore cursor
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }
}

void FreeCamera::cursorPosCallback(GLFWwindow * /*window*/, double xpos, double ypos) {
    if (!current) return;
    if (!current->rightMouseDown) return;

    if (current->firstMouse) {
        current->lastX = xpos;
        current->lastY = ypos;
        current->firstMouse = false;
        return;
    }

    double xoffset = xpos - current->lastX;
    double yoffset = current->lastY - ypos; // reversed: y ranges top->bottom
    current->lastX = xpos;
    current->lastY = ypos;

    float xoff = static_cast<float>(xoffset) * current->mouseSensitivity;
    float yoff = static_cast<float>(yoffset) * current->mouseSensitivity;

    current->yaw += xoff;
    current->pitch += yoff;

    // constrain pitch
    if (current->pitch > 89.0f) current->pitch = 89.0f;
    if (current->pitch < -89.0f) current->pitch = -89.0f;

    current->updateVectors();
}

void FreeCamera::updateVectors() {
    glm::vec3 f;
    f.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    f.y = sin(glm::radians(pitch));
    f.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    front = glm::normalize(f);
    right = glm::normalize(glm::cross(front, worldUp));
    up = glm::normalize(glm::cross(right, front));
}
