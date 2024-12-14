#ifndef _GLTFRENDER_GLCORE_H
#define _GLTFRENDER_GLCORE_H

#include <atomic>
#include <iostream>
#include <string>
#include <vector>

#include <fmt/format.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/fwd.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

class GltfRenderGLCore
{
public:
    GltfRenderGLCore()
    {
        //cameraMtx = glm::scale(cameraMtx, glm::vec3(1/5.0f, 1/5.0f, 1.0f));
        // 相机的设置
    }
    GLFWwindow *window = nullptr;
    float fov = 45.0f;                                  // 初始 FOV
    float aspectRatio = 800.0f / 600.0f;                // 假设窗口大小是800x600
    float m_near = 0.01f;                               // 近裁剪面
    float m_far = 10000.0f;                             // 远裁剪面
    glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 10.0f); // 相机位置
    glm::vec3 target = glm::vec3(0.0f, 0.0f, 0.0f);     // 目标位置（看向原点）
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);         // 上方向为 Y 轴正方向

    glm::mat4 view = glm::lookAt(cameraPos, target, up); // 视图矩阵为单位矩阵
    glm::mat4 projection = glm::perspective(glm::radians(fov), aspectRatio, m_near, m_far); // 投影矩阵为单位矩阵

    glm::vec4 backgroundColor = glm::vec4(1, 1, 1, 0);
    std::atomic_bool statusNeedRefresh = false;
    GLuint shaderProgram = -1;
    void InitShaderProgram();
    void FrameEvent();

    void KeyCallbackForGlfw(int key, int scancode, int action, int mods);

    glm::vec2 m_mousePostionLast = glm::vec2(0,0);
    bool m_mouseLeftRepeat = false;

    float m_cameraPitch = 0.0f; // 初始俯仰角
    float m_cameraYaw = 0.0f;   // 初始偏航角
    void mouse_pos_callback(double xpos, double ypos)
    {
        glm::vec2 pos(xpos, ypos);
        glm::vec2 posMove = pos - this->m_mousePostionLast;
        this->m_mousePostionLast = pos;

        if (this->m_mouseLeftRepeat == true) {
            // 将偏移量转换为旋转角度（调整敏感度）
            float sensitivityX = 0.5; // 灵敏度
            float sensitivityY = 1; // 灵敏度

            float angleX = static_cast<float>(posMove.y) * sensitivityX * -1; // y 偏移控制绕 x 轴旋转
            float angleY = static_cast<float>(posMove.x) * sensitivityY; // x 偏移控制绕 y 轴旋转

            this->m_cameraPitch += angleX;
            this->m_cameraYaw += angleY;
            if (this->m_cameraPitch > 89.0)
                this->m_cameraPitch = 89.0;
            if (this->m_cameraPitch < -89.0)
                this->m_cameraPitch = -89.0;

            // 通过俯仰角和偏航角计算新的相机方向
            glm::vec3 direction;
            direction.x = cos(glm::radians(this->m_cameraPitch)) * cos(glm::radians(this->m_cameraYaw));
            direction.y = sin(glm::radians(this->m_cameraPitch));
            direction.z = cos(glm::radians(this->m_cameraPitch)) * sin(glm::radians(this->m_cameraYaw));
            direction = glm::normalize(direction);

            // 更新相机位置
            cameraPos = target - direction * glm::length(cameraPos - target); // 保持相机与目标的距离不变

            this->statusNeedRefresh.store(true);
        }
    }

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
                this->m_mouseLeftRepeat = true;
                std::cout << "Left mouse button pressed." << std::endl;
                break;
            case BUTTON_ACTION(GLFW_MOUSE_BUTTON_LEFT, GLFW_REPEAT):
                break;
            case BUTTON_ACTION(GLFW_MOUSE_BUTTON_LEFT, GLFW_RELEASE):
                this->m_mouseLeftRepeat = false;
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
        double distanceMoveRaio = yoffset > 0 ? yoffset * (0.5 / 7) : yoffset * (1.0 / 7);
        double scaleRatio = 1.0 - distanceMoveRaio;
        std::cout << fmt::format(" Scroll : ({},{}); {}", xoffset, yoffset, scaleRatio) << std::endl;

        glm::vec3 ToCamera = this->cameraPos - this->target;
        ToCamera = ToCamera * glm::vec3(scaleRatio, scaleRatio, scaleRatio);
        this->cameraPos = this->target + ToCamera;

        this->statusNeedRefresh.store(true);
    }
};

#endif // !_GLTFRENDER_GLCORE_H
