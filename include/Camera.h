#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <GLFW/glfw3.h>

class Camera
{
public:
    glm::vec3 Position, Front, Up, Right, WorldUp;
    float Yaw, Pitch, Speed, Sensitivity, Zoom;

    Camera(glm::vec3 position = { 0.0f, 0.0f, 3.0f });
    glm::mat4 GetViewMatrix();
    void ProcessKeyboard(GLFWwindow* window, float deltaTime);
    void ProcessMouseMovement(float xoffset, float yoffset);
    void ProcessMouseScroll(float yoffset);
private:
    void updateCameraVectors();
};
