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
    // ---------------------------------------------------------------------- 相机
    float fov = 45.0f;                                  // 初始 FOV
    float aspectRatio = 800.0f / 600.0f;                // 假设窗口大小是800x600
    float m_near = 0.01f;                               // 近裁剪面
    float m_far = 10000.0f;                             // 远裁剪面
    glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 10.0f); // 相机位置
    glm::vec3 target = glm::vec3(0.0f, 0.0f, 0.0f);     // 目标位置（看向原点）
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);         // 上方向为 Y 轴正方向
    float m_cameraPitch = 0.0f;                         // 初始俯仰角
    float m_cameraYaw = 0.0f;                           // 初始偏航角

    glm::mat4 view = glm::lookAt(cameraPos, target, up); // 视图矩阵为单位矩阵
    glm::mat4 projection = glm::perspective(glm::radians(fov), aspectRatio, m_near, m_far); // 投影矩阵为单位矩阵

    // --------------------------------------------------------------------- 环境
    glm::vec4 backgroundColor = glm::vec4(1, 1, 1, 0);

    // --------------------------------------------------------------------- OpenGL 同步
    GLFWwindow *window = nullptr;
    std::atomic_bool statusNeedRefresh = false;
    GLuint shaderProgram = -1;
    void FrameEvent();
    void InitShaderProgram();

    // --------------------------------------------------------------------- 键鼠交互
    glm::vec2 m_mousePostionLast = glm::vec2(0,0);
    bool m_mouseLeftRepeat = false;
    void KeyCallbackForGlfw(int key, int scancode, int action, int mods);
    void mouse_pos_callback(double xpos, double ypos);
    void mouse_enter_callback(int entered);
    void mouse_button_callback(int button, int action, int mods);
    void scroll_callback(double xoffset, double yoffset);
};

#endif // !_GLTFRENDER_GLCORE_H
