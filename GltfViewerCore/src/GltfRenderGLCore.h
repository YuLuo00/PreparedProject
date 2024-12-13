#ifndef _GLTFRENDER_GLCORE_H
#define _GLTFRENDER_GLCORE_H

#include <atomic>

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
    float fov = 45.0f;                                 // 初始 FOV
    float aspectRatio = 800.0f / 600.0f;               // 假设窗口大小是800x600
    float m_near = 0.1f;                               // 近裁剪面
    float m_far = 100.0f;                              // 远裁剪面
    glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 3.0f); // 相机位置
    glm::vec3 target = glm::vec3(0.0f, 0.0f, 0.0f);    // 目标位置（看向原点）
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);        // 上方向为 Y 轴正方向

    glm::mat4 view = glm::mat4(1.0f);       // 视图矩阵为单位矩阵
    glm::mat4 projection = glm::mat4(1.0f); // 投影矩阵为单位矩阵
    glm::vec4 backgroundColor = glm::vec4(1, 1, 1, 0);
    std::atomic_bool statusNeedRefresh = false;
    GLuint shaderProgram = -1;
    void InitShaderProgram();
    void FrameEvent();

    void KeyCallbackForGlfw(GLFWwindow *window, int key, int scancode, int action, int mods);
};

#endif // !_GLTFRENDER_GLCORE_H
