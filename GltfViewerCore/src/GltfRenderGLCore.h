#ifndef _GLTFRENDER_GLCORE_H
#define _GLTFRENDER_GLCORE_H

#include <atomic>
#include <iostream>
#include <string>

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/fwd.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <fmt/format.h>

class GltfRenderGLCore
{
public:
    GltfRenderGLCore()
    {
        //cameraMtx = glm::scale(cameraMtx, glm::vec3(1/5.0f, 1/5.0f, 1.0f));
        // 相机的设置
    }
    GLFWwindow *window = nullptr;
    float fov = 45.0f;                                 // 初始 FOV
    float aspectRatio = 800.0f / 600.0f;               // 假设窗口大小是800x600
    float m_near = 0.1f;                               // 近裁剪面
    float m_far = 100.0f;                              // 远裁剪面
    glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 3.0f); // 相机位置
    glm::vec3 target = glm::vec3(0.0f, 0.0f, 0.0f);    // 目标位置（看向原点）
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);        // 上方向为 Y 轴正方向

    glm::mat4 view = glm::mat4(1.0f);       // 视图矩阵为单位矩阵
    glm::mat4 projection = glm::mat4(1.0f); // 投影矩阵为单位矩阵
    glm::mat4 worldMtx = glm::mat4(1.0f);   // 投影矩阵为单位矩阵

    glm::vec4 backgroundColor = glm::vec4(1, 1, 1, 0);
    std::atomic_bool statusNeedRefresh = false;
    GLuint shaderProgram = -1;
    void InitShaderProgram();
    void FrameEvent();

    void KeyCallbackForGlfw(int key, int scancode, int action, int mods);

    void mouse_pos_callback(double xpos, double ypos) { }

    void mouse_enter_callback(int entered)
    {
        if (entered == GLFW_TRUE) {
            std::cout << "mouse enter." << std::endl;
        }
        else if (entered == GLFW_FALSE) {
            std::cout << "mouse leave" << std::endl;
        }
    }

    void mouse_button_callback(int button, int action, int mods)
    {
#define BUTTON_ACTION(BUTTON, ACTION) (BUTTON * 100 + ACTION)

        switch (BUTTON_ACTION(button, action)) {
            case BUTTON_ACTION(GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS):
                std::cout << "Left mouse button pressed." << std::endl;
                break;
            case BUTTON_ACTION(GLFW_MOUSE_BUTTON_LEFT, GLFW_RELEASE):
                std::cout << "Left mouse button release." << std::endl;
                break;
            case BUTTON_ACTION(GLFW_MOUSE_BUTTON_RIGHT, GLFW_PRESS):
                std::cout << "Right mouse button pressed." << std::endl;
                break;
            case BUTTON_ACTION(GLFW_MOUSE_BUTTON_RIGHT, GLFW_RELEASE):
                std::cout << "RIGHT mouse button RELEASE." << std::endl;
                break;
            case BUTTON_ACTION(GLFW_MOUSE_BUTTON_4, GLFW_PRESS):
                std::cout << "BUTTON_4 mouse button PRESS." << std::endl;
                break;
            case BUTTON_ACTION(GLFW_MOUSE_BUTTON_4, GLFW_RELEASE):
                std::cout << "BUTTON_4 mouse button RELEASE." << std::endl;
                break;
            case BUTTON_ACTION(GLFW_MOUSE_BUTTON_5, GLFW_PRESS):
                std::cout << "BUTTON_5 mouse button PRESS." << std::endl;
                break;
            case BUTTON_ACTION(GLFW_MOUSE_BUTTON_5, GLFW_RELEASE):
                std::cout << "BUTTON_5 mouse button RELEASE." << std::endl;
                break;
            default:
                break;
        }
    }

    void scroll_callback(double xoffset, double yoffset)
    {
        double scaleRatio = yoffset > 0 ? yoffset * (1.0 / 7) : yoffset * (0.5 / 7);
        scaleRatio = 1.0 + scaleRatio;
        std::cout<< fmt::format(" Scroll : ({},{}); {}", xoffset, yoffset, scaleRatio)<< std::endl;

        this->worldMtx = glm::scale(this->worldMtx, glm::vec3(scaleRatio, scaleRatio, scaleRatio));
        this->statusNeedRefresh.store(true);
    }
};

#endif // !_GLTFRENDER_GLCORE_H
