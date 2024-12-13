#include "GltfRenderGLCore.h"

#include <iostream>
#include <map>
#include <vector>

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <opencv2/opencv.hpp>
#include <tiny_gltf.h>

#include "GltfRender.h"
#include "GLGeometry.h"

// 顶点着色器源码
static const char *vertexShaderSource = R"(
#version 330 core
layout(location = 0) in vec3 position; // 输入顶点位置
layout(location = 1) in vec3 color;    // 输入顶点颜色

uniform mat4 view; // 声明视图矩阵uniform变量
uniform mat4 projection;

out vec3 fragColor; // 输出到片段着色器

void main() {
    gl_Position = vec4(position, 1.0); // 设置顶点位置
    gl_Position = projection * view * vec4(position, 1.0);
    fragColor = color; // 将颜色传递给片段着色器
}
)";

// 片段着色器源码
static const char *fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;
void main() {
    FragColor = vec4(1.0, 0.5, 0.2, 1.0);
}
)";

// 检查编译和链接错误
void checkCompileErrors1(GLuint shader, std::string type)
{
    GLchar char123;
    GLint success;
    GLchar infoLog[1024];
    if (type != "PROGRAM") {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(shader, 1024, NULL, infoLog);
            std::cerr << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n" << infoLog << "\n";
        }
    }
    else {
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(shader, 1024, NULL, infoLog);
            std::cerr << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n" << infoLog << "\n";
        }
    }
}


void GltfRenderGLCore::FrameEvent()
{
    static GLint viewLoc = -1;
    static GLint projectionLoc = -1;
    static bool initFlag = false;
    if (initFlag == false) {
        initFlag = true;
        // 获取 view 和 projection uniform 变量的位置
        viewLoc = glGetUniformLocation(this->shaderProgram, "view");
        projectionLoc = glGetUniformLocation(this->shaderProgram, "projection");

        this->projection = glm::perspective(glm::radians(fov), aspectRatio, m_near, m_far);
        this->view = glm::lookAt(cameraPos, target, up);

        // 将单位矩阵传递给着色器中的 uniform 变量
        // 相机的设置
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(this->view));             // 设置 view 矩阵
        glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(this->projection)); // 设置 projection 矩阵
    }

    // 检查按键事件
    if (this->statusNeedRefresh.load() == true) {
        this->statusNeedRefresh.store(false);
        this->projection = glm::perspective(glm::radians(fov), aspectRatio, m_near, m_far);

        glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(this->projection)); // 设置 projection 矩阵
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(this->view));
    }
}


void GltfRenderGLCore::KeyCallbackForGlfw(GLFWwindow *window, int key, int scancode, int action, int mods)
{
#define KEY_ACTION(KEY, ACTION) (KEY * 10000 + ACTION)
    switch (KEY_ACTION(key, action)) {
        case KEY_ACTION(GLFW_KEY_D, GLFW_PRESS):
            this->view = glm::translate(this->view, glm::vec3(0.1f, 0.0f, 0.0f));
            this->statusNeedRefresh.exchange(true);
            break;
        case KEY_ACTION(GLFW_KEY_A, GLFW_PRESS):
            this->view = glm::translate(this->view, glm::vec3(-0.1f, 0.0f, 0.0f));
            this->statusNeedRefresh.exchange(true);
            break;
        case KEY_ACTION(GLFW_KEY_W, GLFW_PRESS):
            this->fov += 10;
            this->statusNeedRefresh.exchange(true);
            break;
        case KEY_ACTION(GLFW_KEY_S, GLFW_PRESS):
            this->fov -= 10;
            this->statusNeedRefresh.exchange(true);
            break;
        default:
            break;
    }
}

void GltfRenderGLCore::InitShaderProgram()
{
    // 编译顶点着色器
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    checkCompileErrors1(vertexShader, "VERTEX");

    // 编译片段着色器
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    checkCompileErrors1(fragmentShader, "FRAGMENT");

    // 链接着色器程序
    this->shaderProgram = glCreateProgram();
    glAttachShader(this->shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    checkCompileErrors1(shaderProgram, "PROGRAM");

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}



