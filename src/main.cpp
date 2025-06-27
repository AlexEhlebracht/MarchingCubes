#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <iostream>

#include "../include/Shader.h"
#include "../include/Camera.h"
#include "../include/World.h"

Camera camera(glm::vec3(40.0f, 80.0f, 40.0f));
float lastX = 800.0f / 2.0f;
float lastY = 600.0f / 2.0f;
bool firstMouse = true;

float deltaTime = 0.0f;
float lastFrame = 0.0f;
bool leftMousePressed = false;
bool rightMousePressed = false;
float lastDeformTime = 0.0f;
const float DEFORM_INTERVAL = 0.05f;

float fpsTimer = 0.0f;
int frameCount = 0;

glm::mat4 getProjectionMatrix(float width, float height)
{
    return glm::perspective(glm::radians(90.0f), width / height, 0.1f, 10000.0f);
}

void framebuffer_size_callback(GLFWwindow*, int width, int height)
{
    glViewport(0, 0, width, height);
}

void processInput(GLFWwindow* window, World& world)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    camera.ProcessKeyboard(window, deltaTime);
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }
    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;
    camera.ProcessMouseMovement(xoffset, yoffset);
}

void scroll_callback(GLFWwindow* window, double /*xoffset*/, double yoffset)
{
    camera.ProcessMouseScroll(yoffset);
}

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4); // 4x MSAA

    GLFWwindow* window = glfwCreateWindow(800, 600, "Marching Cubes Demo", nullptr, nullptr);
    if (!window)
    {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(0);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cerr << "Failed to initialize GLAD\n";
        return -1;
    }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glEnable(GL_DITHER);
    glEnable(GL_MULTISAMPLE);
    //glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    Shader shader("res/shaders/mc.vert", "res/shaders/mc.frag");
    World world;

    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        frameCount++;
        fpsTimer += deltaTime;
        if (fpsTimer >= 1.0f)
        {
            std::cout << "FPS: " << frameCount / fpsTimer << "\n";
            frameCount = 0;
            fpsTimer = 0.0f;
        }

        world.update(camera.Position);
        processInput(window, world);

        glClearColor(0.53f, 0.81f, 0.92f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader.use();
        glm::mat4 view = camera.GetViewMatrix();
        glm::mat4 projection = getProjectionMatrix(800.0f, 600.0f);
        shader.setMat4("view", view);
        shader.setMat4("projection", projection);
        glm::mat4 model = glm::mat4(1.0f);
        shader.setMat4("model", model);
        shader.setVec3("lightDir", glm::normalize(glm::vec3(-0.7f, -0.7f, -0.7f)));
        shader.setVec3("viewPos", camera.Position);

        world.draw(shader, camera.Position, view, projection); // Updated call

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}