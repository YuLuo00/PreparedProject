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
#include "GltfRenderGLCore.h"

static std::map<GLFWwindow *, GltfRender *> g_window2render;

// 处理鼠标移动事件的回调函数
void mouse_pos_callback(GLFWwindow *window, double xpos, double ypos) { }

void mouse_enter_callback(GLFWwindow *window, int entered)
{
    return g_window2render.at(window)->m_data->mouse_enter_callback(entered);
}

void mouse_button_callback(GLFWwindow *window, int button, int action, int mods)
{
    return g_window2render.at(window)->m_data->mouse_button_callback(button, action, mods);
}

void scroll_callback(GLFWwindow *window, double xoffset, double yoffset)
{
    return g_window2render.at(window)->m_data->scroll_callback(xoffset, yoffset);
}

// 按键回调函数
void glfw_key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    g_window2render.at(window)->m_data->KeyCallbackForGlfw(key, scancode, action, mods);
}

GltfRender::GltfRender()
{
    this->m_data = new GltfRenderGLCore();
}

GLFWwindow *InitWindowForGlfw()
{
    GLFWwindow *window = nullptr;
    // 初始化 GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW!" << std::endl;
        return nullptr;
    }

    //// 隐藏窗口
    //glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    // 创建窗口
    window = glfwCreateWindow(800, 600, "Offscreen", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create GLFW window!" << std::endl;
        glfwTerminate();
        return false;
    }
    glfwMakeContextCurrent(window);

    // 初始化 GLEW
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW!" << std::endl;
        return nullptr;
    }

    return window;
}

int GltfRender::Run()
{
    GLFWwindow *window = InitWindowForGlfw();
    if (window == nullptr) {
        return -1;
    }

    // 注册鼠标回调函数
    g_window2render[window] = this;
    glfwSetCursorPosCallback(window, mouse_pos_callback);
    glfwSetCursorEnterCallback(window, mouse_enter_callback);
    glfwSetKeyCallback(window, glfw_key_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetScrollCallback(window, scroll_callback);

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
    while (!glfwWindowShouldClose(window)) {
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
        glfwSwapBuffers(window);
    }

    // 清理资源
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(this->m_data->shaderProgram);
    glfwDestroyWindow(window);
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