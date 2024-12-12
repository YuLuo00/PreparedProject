#include "GltfRender.h"

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
/*

#version 330 core
layout(location = 0) in vec3 position;
uniform mat4 view; // 声明视图矩阵uniform变量
uniform mat4 projection;
void main()
{
    gl_Position = projection * view * vec4(position, 1.0);
}

*/

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

class GltfRenderGLCore
{
public:
    GltfRenderGLCore() = default;
    GLFWwindow *window = nullptr;

    glm::mat4 cameraMtx = glm::mat4(1.0f);  // 视图矩阵为单位矩阵
    glm::mat4 projection = glm::mat4(1.0f); // 投影矩阵为单位矩阵
    glm::vec4 backgroundColor = glm::vec4(1, 1, 1, 0);
    std::atomic_bool statusNeedRefresh = false;

    void KeyCallbackForGlfw(GLFWwindow *window, int key, int scancode, int action, int mods);
};

GltfRender::GltfRender()
{
    this->m_data = new GltfRenderGLCore();
}

bool GltfRender::Init()
{
    // 初始化 GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW!" << std::endl;
        return false;
    }

    //// 隐藏窗口
    //glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    // 创建窗口
    this->m_data->window = glfwCreateWindow(800, 600, "Offscreen", NULL, NULL);
    if (!this->m_data->window) {
        std::cerr << "Failed to create GLFW window!" << std::endl;
        glfwTerminate();
        return false;
    }
    glfwMakeContextCurrent(this->m_data->window);

    // 初始化 GLEW
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW!" << std::endl;
        return false;
    }

    return true;
}

// 摄像机的旋转角度
float yaw = -90.0f;     // 水平旋转角度（绕y轴旋转）
float pitch = 0.0f;     // 垂直旋转角度（绕x轴旋转）
float lastX = 400.0f;   // 上一次的鼠标 x 坐标
float lastY = 300.0f;   // 上一次的鼠标 y 坐标
bool firstMouse = true; // 是否是第一次捕获鼠标

// 处理鼠标移动事件的回调函数
void mouse_callback(GLFWwindow *window, double xpos, double ypos)
{
    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xOffset = xpos - lastX; // 计算鼠标在x轴的偏移量
    float yOffset = lastY - ypos; // 计算鼠标在y轴的偏移量
    lastX = xpos;                 // 更新上次的鼠标位置
    lastY = ypos;

    const float sensitivity = 0.05f; // 鼠标灵敏度，调整旋转速度
    xOffset *= sensitivity;
    yOffset *= sensitivity;

    yaw += xOffset;   // 更新水平旋转角度
    pitch += yOffset; // 更新垂直旋转角度

    // 限制pitch角度（避免翻转）
    if (pitch > 89.0f)
        pitch = 89.0f;
    if (pitch < -89.0f)
        pitch = -89.0f;
}

static std::map<GLFWwindow *, GltfRender *> g_window2render;
// 按键回调函数
void glfw_key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    if (g_window2render.count(window) < 0) {
        return;
    }

    g_window2render.at(window)->m_data->KeyCallbackForGlfw(window, key, scancode, action, mods);
}

void GltfRenderGLCore::KeyCallbackForGlfw(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    switch (key) {
        case GLFW_KEY_D:
            if (action == GLFW_PRESS) {
                this->cameraMtx = glm::translate(this->cameraMtx, glm::vec3(0.1f, 0.0f, 0.0f));
                this->statusNeedRefresh.exchange(true);
            }
            break;
        case GLFW_KEY_A:
            if (action == GLFW_PRESS) {
                this->cameraMtx = glm::translate(this->cameraMtx, glm::vec3(-0.1f, 0.0f, 0.0f));
                this->statusNeedRefresh.exchange(true);
            }
            break;
        default:
            break;
    }
}

int GltfRender::Run()
{
    if (Init() == false) {
        return -1;
    }

    // 注册鼠标回调函数
    glfwSetCursorPosCallback(this->m_data->window, mouse_callback);
    g_window2render[this->m_data->window] = this;
    glfwSetKeyCallback(this->m_data->window, glfw_key_callback);
    // 顶点数据，两个三角形
    float vertices[] = {
        // 第一个三角形
        -0.5f,
        -0.5f,
        0.0f, // 左下角
        0.5f,
        -0.5f,
        0.0f, // 右下角
        0.0f,
        0.5f,
        0.0f, // 顶部

        // 第二个三角形
        -0.5f,
        -0.5f,
        0.0f, // 左下角
        0.5f,
        -0.5f,
        0.0f, // 右下角
        0.0f,
        0.5f,
        0.0f // 顶部
    };

    // 创建顶点数组对象 (VAO) 和顶点缓冲对象 (VBO)
    GLuint VBO, VAO;
    glGenVertexArrays(1, &VAO); // 生成一个 VAO，保存到 VAO 变量中
    glGenBuffers(1, &VBO);      // 生成一个 VBO，保存到 VBO 变量中

    // 绑定 VAO（将后续的设置记录到该 VAO）
    glBindVertexArray(VAO); // 绑定 VAO，后续操作会与此 VAO 相关联

    // 绑定 VBO（指定该缓冲区用于存储顶点数据）
    glBindBuffer(GL_ARRAY_BUFFER, VBO); // 绑定 VBO，用于顶点数据存储

    // 将顶点数据传递到 VBO 中
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    // 将顶点数据从 `vertices` 数组复制到 VBO 中，大小为 `sizeof(vertices)`，使用 `GL_STATIC_DRAW` 表示数据不会频繁更新

    // 设置顶点属性指针，告诉 OpenGL 如何解析 VBO 中的数据
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
    // 绑定到属性位置 0（顶点坐标），每个顶点 3 个浮动值（`GL_FLOAT`），
    // 间距为 3 * sizeof(float)，表示每个顶点由 3 个 float 数值组成，起始位置为 0（即 `vertices` 数组的开始）

    // 启用顶点属性数组 0
    glEnableVertexAttribArray(0); // 启用顶点属性位置 0，以便在着色器中使用

    // 解绑 VBO 和 VAO，恢复到默认状态
    glBindBuffer(GL_ARRAY_BUFFER, 0); // 解除绑定当前的 VBO
    glBindVertexArray(0);             // 解除绑定当前的 VAO，表示不再使用此 VAO

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
    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    checkCompileErrors1(shaderProgram, "PROGRAM");

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // 设置帧缓冲大小
    glViewport(0, 0, 800, 600);

    // 存储渲染结果的容器
    std::vector<cv::Mat> frames;

    glUseProgram(shaderProgram);
    glBindVertexArray(VAO);

    // 获取 view 和 projection uniform 变量的位置
    GLint viewLoc = glGetUniformLocation(shaderProgram, "view");
    if (viewLoc == -1) {
        return 2;
    }

    GLint projectionLoc = glGetUniformLocation(shaderProgram, "projection");
    // 将单位矩阵传递给着色器中的 uniform 变量
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(this->m_data->cameraMtx));        // 设置 view 矩阵
    glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(this->m_data->projection)); // 设置 projection 矩阵

    // 渲染循环
    while (!glfwWindowShouldClose(this->m_data->window)) {
        // 处理窗口事件
        glfwPollEvents(); // 处理事件，比如键盘、鼠标输入等

        // 检查按键事件
        if (this->m_data->statusNeedRefresh.load() == true) {
            this->m_data->statusNeedRefresh.store(false);
            glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(this->m_data->cameraMtx));
        }

        // 设置清屏颜色
        glClearColor(this->m_data->backgroundColor[0],
                     this->m_data->backgroundColor[1],
                     this->m_data->backgroundColor[2],
                     this->m_data->backgroundColor[3]);
        glClear(GL_COLOR_BUFFER_BIT);

        // 绘制三角形
        glDrawArrays(GL_TRIANGLES, 0, 3);

        //// 读取像素数据
        //cv::Mat image(600, 800, CV_8UC3);
        //glReadPixels(0, 0, 800, 600, GL_BGR, GL_UNSIGNED_BYTE, image.data);
        //cv::flip(image, image, 0); // 翻转图像
        //frames.push_back(image);

        // 刷新窗口，交换缓冲区
        glfwSwapBuffers(this->m_data->window);
    }

    // 保存帧为图像文件
    for (size_t i = 0; i < frames.size(); ++i) {
        std::string filename = "frame_" + std::to_string(i) + ".png";
        cv::imwrite(filename, frames[i]);
    }

    // 清理资源
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);
    glfwDestroyWindow(this->m_data->window);
    glfwTerminate();

    return 0;
}

bool GltfRender::LoadGltf(const std::string &filePath)
{
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err, warn;
    if (!loader.LoadASCIIFromFile(&model, &err, &warn, filePath)) {
        return false;
    };

    return false;
}