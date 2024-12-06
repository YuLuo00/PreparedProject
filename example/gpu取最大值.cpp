#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>

const char* computeShaderSource = R"(
#version 430
layout(local_size_x = 1) in;
layout(std430, binding = 0) buffer DataBuffer {
    int data[];
};
layout(std430, binding = 1) buffer MaxBuffer {
    int maxValue; // 存储最大值
};

void main() {
    uint id = gl_GlobalInvocationID.x;
    int value = data[id];

    // 如果当前值大于 maxValue，则更新 maxValue
    atomicMax(maxValue, value);
}
)";

int main() {
    // 初始化 GLFW 和 GLEW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    //GLFWwindow* window = glfwCreateWindow(800, 600, "Compute Shader Demo", nullptr, nullptr);
    GLFWwindow* window = glfwCreateWindow(0, 0, "Hidden Window", nullptr, nullptr);

    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glewInit();

    // 创建数据
    std::vector<int> data = { 1, 5, 3, 7, 2, 8 }; // 示例数据
    int maxValue = 0; // 初始最大值

    // 创建缓冲区
    GLuint dataBuffer, maxBuffer;
    glGenBuffers(1, &dataBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, dataBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, data.size() * sizeof(int), data.data(), GL_STATIC_DRAW);

    glGenBuffers(1, &maxBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, maxBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(int), &maxValue, GL_STATIC_DRAW);

    // 创建计算着色器
    GLuint computeShader = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(computeShader, 1, &computeShaderSource, nullptr);
    glCompileShader(computeShader);

    // 检查编译错误
    GLint success;
    glGetShaderiv(computeShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(computeShader, 512, nullptr, infoLog);
        std::cerr << "Compute Shader Compilation Failed: " << infoLog << std::endl;
    }

    // 创建程序并链接
    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, computeShader);
    glLinkProgram(shaderProgram);

    // 绑定缓冲区
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, dataBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, maxBuffer);

    // 执行计算着色器
    glUseProgram(shaderProgram);
    glDispatchCompute(data.size(), 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT); // 确保数据写入完成

    // 读取结果
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, maxBuffer);
    int* ptr = (int*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
    if (ptr) {
        std::cout << "Max Value: " << *ptr << std::endl;
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    }

    // 清理
    glDeleteProgram(shaderProgram);
    glDeleteShader(computeShader);
    glDeleteBuffers(1, &dataBuffer);
    glDeleteBuffers(1, &maxBuffer);
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
