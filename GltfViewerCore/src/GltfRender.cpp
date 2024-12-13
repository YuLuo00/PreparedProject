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
    GltfRenderGLCore()
    {
        //cameraMtx = glm::scale(cameraMtx, glm::vec3(1/5.0f, 1/5.0f, 1.0f));
    }
    GLFWwindow *window = nullptr;

    glm::mat4 cameraMtx = glm::mat4(1.0f);  // 视图矩阵为单位矩阵
    glm::mat4 projection = glm::mat4(1.0f); // 投影矩阵为单位矩阵
    glm::vec4 backgroundColor = glm::vec4(1, 1, 1, 0);
    std::atomic_bool statusNeedRefresh = false;
    GLuint shaderProgram = -1;
    void InitShaderProgram();
    void FrameEvent();

    void KeyCallbackForGlfw(GLFWwindow *window, int key, int scancode, int action, int mods);
};

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

// 处理鼠标移动事件的回调函数
void mouse_callback(GLFWwindow *window, double xpos, double ypos) { }

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
#define KEY_ACTION(KEY, ACTION) (KEY * 10000 + ACTION)
    switch (KEY_ACTION(key, action)) {
        case KEY_ACTION(GLFW_KEY_D, GLFW_PRESS):
            this->cameraMtx = glm::translate(this->cameraMtx, glm::vec3(0.1f, 0.0f, 0.0f));
            this->statusNeedRefresh.exchange(true);
            break;
        case KEY_ACTION(GLFW_KEY_A, GLFW_PRESS):
            this->cameraMtx = glm::translate(this->cameraMtx, glm::vec3(-0.1f, 0.0f, 0.0f));
            this->statusNeedRefresh.exchange(true);
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
    int numVertex = 2;
    std::vector<VertexInfo> vertices(6);
    vertices[0].position = VertexInfo::Position(-1, 0, 0);
    vertices[1].position = VertexInfo::Position(-0.1, 1, 0);
    vertices[2].position = VertexInfo::Position(-0.1, 0, 0);
    vertices[3].position = VertexInfo::Position(1, 0, 0);
    vertices[4].position = VertexInfo::Position(0.1, 1, 0);
    vertices[5].position = VertexInfo::Position(0.1, 0, 0);

    // 创建顶点数组对象 (VAO) 和顶点缓冲对象 (VBO)
    GLuint VBO, VAO;
    glGenVertexArrays(1, &VAO); // 生成一个 VAO，保存到 VAO 变量中
    glGenBuffers(1, &VBO);      // 生成一个 VBO，保存到 VBO 变量中

    glBindVertexArray(VAO);             // 绑定 VAO，后续操作会与此 VAO 相关联
    glBindBuffer(GL_ARRAY_BUFFER, VBO); // 绑定 VBO，用于顶点数据存储
    {
        glBufferStorage(
            GL_ARRAY_BUFFER, sizeof(vertices[0]) * vertices.size(), NULL, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT);

        // 持久化映射
        void *mappedPtr = glMapBufferRange(
            GL_ARRAY_BUFFER, 0, sizeof(vertices[0]) * vertices.size(), GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT);
        if (!mappedPtr) {
            std::cerr << "Failed to map buffer!" << std::endl;
            return -1;
        }
        // 修改数据（直接操作 mappedData）
        memcpy(mappedPtr, vertices.data(), sizeof(vertices[0]) * vertices.size());
        // 刷新缓冲区的数据，确保写入的数据会正确传递到 GPU
        glFlushMappedBufferRange(GL_ARRAY_BUFFER, 0, sizeof(vertices[0]) * vertices.size());
    }
    glVertexAttribPointer(0,
                          sizeof(VertexInfo::Position) / sizeof(float),
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(vertices[0]),
                          (void *)offsetof(VertexInfo, position));

    // 启用顶点属性数组 0
    glEnableVertexAttribArray(0); // 启用顶点属性位置 0，以便在着色器中使用

    // 解绑 VBO 和 VAO，恢复到默认状态
    glBindBuffer(GL_ARRAY_BUFFER, 0); // 解除绑定当前的 VBO
    glBindVertexArray(0);             // 解除绑定当前的 VAO，表示不再使用此 VAO

    // 编译顶点着色器
    this->m_data->InitShaderProgram();

    // 设置帧缓冲大小
    glViewport(0, 0, 800, 600);

    // 存储渲染结果的容器
    std::vector<cv::Mat> frames;

    glUseProgram(this->m_data->shaderProgram);
    glBindVertexArray(VAO);

    // 渲染循环
    while (!glfwWindowShouldClose(this->m_data->window)) {
        // 处理窗口事件
        glfwPollEvents(); // 处理事件，比如键盘、鼠标输入等

        this->m_data->FrameEvent();

        // 设置清屏颜色
        glClearColor(this->m_data->backgroundColor[0],
                     this->m_data->backgroundColor[1],
                     this->m_data->backgroundColor[2],
                     this->m_data->backgroundColor[3]);
        glClear(GL_COLOR_BUFFER_BIT);

        // 绘制三角形
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // 刷新窗口，交换缓冲区
        glfwSwapBuffers(this->m_data->window);
    }

    // 清理资源
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(this->m_data->shaderProgram);
    glfwDestroyWindow(this->m_data->window);
    glfwTerminate();

    return 0;
}

void GltfRenderGLCore::FrameEvent()
{
    static GLint viewLoc = -1;
    static bool initFlag = false;
    if (!initFlag) {
        // 获取 view 和 projection uniform 变量的位置
        viewLoc = glGetUniformLocation(this->shaderProgram, "view");
        if (viewLoc == -1) {
            return;
        }

        GLint projectionLoc = glGetUniformLocation(this->shaderProgram, "projection");
        // 将单位矩阵传递给着色器中的 uniform 变量
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(this->cameraMtx));        // 设置 view 矩阵
        glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(this->projection)); // 设置 projection 矩阵
    }

    // 检查按键事件
    if (this->statusNeedRefresh.load() == true) {
        this->statusNeedRefresh.store(false);
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(this->cameraMtx));
    }
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