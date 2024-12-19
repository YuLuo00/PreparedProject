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
//#include <SDL2/SDL.h>
//#include <SDL2/SDL_opengl.h> // 如果你使用 OpenGL
#include <tiny_gltf.h>

#include <stb_image.h>
#include <stb_image_resize.h>

#include "GLGeometry.h"
#include "GltfRenderGLCore.h"

static std::map<GLFWwindow *, GltfRender *> g_window2render;

// 处理鼠标移动事件的回调函数
void mouse_pos_callback(GLFWwindow *window, double xpos, double ypos)
{
    return g_window2render.at(window)->m_data->mouse_pos_callback(xpos, ypos);
}

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
    //// 创建上下文时指定为核心上下文（4.3或更高版本）
    //SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE); // 使用核心模式
    //SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    //SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

    GLFWwindow *window = nullptr;
    // 初始化 GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW!" << std::endl;
        return nullptr;
    }
    // 确保请求一个支持 OpenGL 4.3 的上下文
    //glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    //glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 核心配置
    //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);  // 对于 MacOS 和某些系统

    //// 隐藏窗口
    //glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 强制使用核心配置文件

    // 创建窗口
    window = glfwCreateWindow(800, 600, "Offscreen", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create GLFW window!" << std::endl;
        glfwTerminate();
        return false;
    }
    glfwMakeContextCurrent(window);
    //glfwSwapInterval(0); // 禁用 V-Sync

    // 初始化 GLEW
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW!" << std::endl;
        return nullptr;
    }

    return window;
}

GLuint LoadTexture(const char *filepath)
{
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    // 读取图片文件
    int width, height, nrChannels;
    unsigned char *data = stbi_load(filepath, &width, &height, &nrChannels, 0);
    if (data) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    else {
        std::cerr << "Failed to load texture: " << filepath << std::endl;
    }
    stbi_image_free(data);

    // 设置纹理过滤方式
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    return textureID;
}

int GltfRender::Run()
{
    GLFWwindow *window = InitWindowForGlfw();
    if (window == nullptr) {
        return -1;
    }

    // 注册鼠标回调函数
    g_window2render[window] = this;
    glfwSetKeyCallback(window, glfw_key_callback);

    glfwSetCursorPosCallback(window, mouse_pos_callback);
    glfwSetCursorEnterCallback(window, mouse_enter_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetScrollCallback(window, scroll_callback);

    // 顶点数据，两个三角形
    int numVertex = 2;
    std::vector<VertexInfo> vertices(6);
    vertices[0].position = glm::vec3(-1, 0, 0);
    vertices[0].uv = glm::vec2(0, 0);
    vertices[0].textrueId = 0;
    vertices[1].position = glm::vec3(-0, 1, 0);
    vertices[1].uv = glm::vec2(1, 1);
    vertices[1].textrueId = 0;
    vertices[2].position = glm::vec3(-0, 0, 0);
    vertices[2].uv = glm::vec2(1, 0);
    vertices[2].textrueId = 0;
    vertices[3].position = glm::vec3(0, 0, 0);
    vertices[3].uv = glm::vec2(0, 0);
    vertices[3].textrueId = 1;
    vertices[4].position = glm::vec3(0, 1, 0);
    vertices[4].uv = glm::vec2(0, 1);
    vertices[4].textrueId = 1;
    vertices[5].position = glm::vec3(1, 0, 0);
    vertices[5].uv = glm::vec2(1, 0);
    vertices[5].textrueId = 1;

    std::vector<InstanceInfo> insts(2);
    insts[0].vertexInfoIdx = {0, 1, 2};
    insts[0].mtx = glm::translate(insts[0].mtx, glm::vec3(0, 1, 0));
    insts[1].vertexInfoIdx = {0, 1, 2};
    insts[1].mtx = glm::translate(insts[1].mtx, glm::vec3(-1, 0, 0));

    // 创建顶点数组对象 (VAO) 和顶点缓冲对象 (VBO)
    GLuint VBO, VAO, InstanceVBO;
    glGenVertexArrays(1, &VAO); // 生成一个 VAO，保存到 VAO 变量中
    glGenBuffers(1, &VBO);      // 生成一个 VBO，保存到 VBO 变量中
    glGenBuffers(1, &InstanceVBO);

    glBindVertexArray(VAO); // 绑定 VAO，后续操作会与此 VAO 相关联

    // 持久化映射 vt
    glBindBuffer(GL_ARRAY_BUFFER, VBO); // 绑定 VBO，用于顶点数据存储
    int verticesSize = sizeof(vertices[0]) * vertices.size();
    glBufferStorage(GL_ARRAY_BUFFER, verticesSize, NULL, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT);
    void *vtMap = glMapBufferRange(GL_ARRAY_BUFFER, 0, verticesSize, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT);
    memcpy(vtMap, vertices.data(), verticesSize); // 修改数据（直接操作 mappedData）
    glFlushMappedBufferRange(GL_ARRAY_BUFFER, 0, verticesSize); // 刷新缓冲区的数据，确保写入的数据会正确传递到 GPU
    // 启用顶点属性数组 0
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VertexInfo), (void*)offsetof(VertexInfo, position));
    glEnableVertexAttribArray(0); // 启用顶点属性位置 0，以便在着色器中使用
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(VertexInfo), (void*)offsetof(VertexInfo, uv));
    glEnableVertexAttribArray(1); // 启用纹理坐标属性
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(VertexInfo), (void*)offsetof(VertexInfo, textrueId));
    glEnableVertexAttribArray(2); // 启用纹理ID属性

    // 持久化映射 inst
    glBindBuffer(GL_ARRAY_BUFFER, InstanceVBO);
    int instanceDataSize = sizeof(InstanceInfo) * insts.size();
    glBufferStorage(GL_ARRAY_BUFFER, instanceDataSize, NULL, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT);
    void *instsMap = glMapBufferRange(GL_ARRAY_BUFFER, 0, instanceDataSize, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT);
    memcpy(instsMap, insts.data(), instanceDataSize);
    glFlushMappedBufferRange(GL_ARRAY_BUFFER, 0, verticesSize);

    glVertexAttribPointer(3, 16, GL_FLOAT, GL_FALSE, sizeof(InstanceInfo), (void*)offsetof(InstanceInfo, mtx));
    glEnableVertexAttribArray(3); // 启用矩阵实例
    glVertexAttribDivisor(3, 1);  // 每个实例使用不同的变换矩阵

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
    //if(0)
    {
        GLuint textureID = -1;
        //生成纹理
        glGenTextures(1, &textureID);
        //绑定纹理
        glBindTexture(GL_TEXTURE_2D_ARRAY, textureID); //绑定时选择纹理数组类型
        //设置如何从数据缓冲区去读取图像数据
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        //设置纹理过滤的参数
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        //两种分配方式都可以，glTexImage3D好像是老版本的方式
        glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, 1000, 1000, 2);
        //glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGB, 1000, 1000, 2, 0,GL_RGB, GL_UNSIGNED_BYTE, NULL);

        const char *path[2] = {"texture0.jpg", "texture1.jpg"};
        //加载图片
        for (int i = 0; i < 2; i++) {
            // 使用 OpenCV 读取图片
            cv::Mat image = cv::imread(path[i], cv::IMREAD_COLOR); // 读取彩色图像
            cv::cvtColor(image, image, cv::COLOR_BGR2RGBA);        // 将 BGR 转换为 RGBA

            if (image.empty()) {
                std::cerr << "Failed to load image: " << path[i] << std::endl;
                continue;
            }

            // 使用 OpenCV 缩放图像到 1000x1000
            cv::Mat resizedImage;
            cv::resize(image, resizedImage, cv::Size(1000, 1000)); // 目标尺寸 1000x1000

            // 更新纹理数据，OpenGL 纹理需要传入原始的 uchar 数组数据
            // OpenCV 读取的图像是 BGR 格式，如果是 RGBA 格式，你可以使用 cv::cvtColor 转换为 RGBA
            // y轴翻转
            cv::flip(resizedImage, resizedImage, 0);

            // 更新 OpenGL 纹理
            glTexSubImage3D(
                GL_TEXTURE_2D_ARRAY, 0, 0, 0, i, 1000, 1000, 1, GL_RGBA, GL_UNSIGNED_BYTE, resizedImage.data);
        }

        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D_ARRAY, textureID);
        glUniform1i(glGetUniformLocation(this->m_data->shaderProgram, "TextureArray"), 2); //纹理传入binding=2
    }

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
    //tinygltf::Model model;
    //tinygltf::TinyGLTF loader;
    //std::string err, warn;
    //if (!loader.LoadASCIIFromFile(&model, &err, &warn, filePath)) {
    //    return false;
    //};

    return false;
}