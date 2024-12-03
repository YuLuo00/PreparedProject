#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <vector>

const char* computeShaderSource = R"(
#version 430
layout(local_size_x = 1) in;

layout(std430, binding = 0) buffer Matrices {
    mat4 matrices[];
};

layout(std430, binding = 1) buffer Coordinates {
    vec4 coordinates[];
};

layout(std430, binding = 2) buffer Results {
    vec4 results[];
};

void main() {
    uint id = gl_GlobalInvocationID.x;
    results[id] = matrices[id] * coordinates[id];
}
)";

int main() {
    // 初始化 GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    // 创建 OpenGL 上下文
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    GLFWwindow* window = glfwCreateWindow(800, 600, "Matrix Multiplication", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glewInit();

    // 创建计算着色器
    GLuint computeShader = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(computeShader, 1, &computeShaderSource, nullptr);
    glCompileShader(computeShader);

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, computeShader);
    glLinkProgram(shaderProgram);
    glDeleteShader(computeShader); // 链接后可以删除计算着色器

    // 矩阵和坐标数据
    std::vector<glm::mat4> matrices = {
        glm::mat4(1.0f), // 单位矩阵
        glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 0.0f, 0.0f)), // 平移矩阵
        glm::rotate(glm::mat4(1.0f), glm::radians(45.0f), glm::vec3(0.0f, 0.0f, 1.0f)) // 旋转矩阵
    };

    std::vector<glm::vec4> coordinates = {
        glm::vec4(0.0f, 0.0f, 0.0f, 1.0f),
        glm::vec4(1.0f, 1.0f, 0.0f, 1.0f),
        glm::vec4(2.0f, 2.0f, 0.0f, 1.0f)
    };

    std::vector<glm::vec4> results(coordinates.size());

    // 创建并绑定缓冲区对象
    GLuint matrixBuffer, coordinateBuffer, resultBuffer;

    glGenBuffers(1, &matrixBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, matrixBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, matrices.size() * sizeof(glm::mat4), matrices.data(), GL_STATIC_DRAW);

    glGenBuffers(1, &coordinateBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, coordinateBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, coordinates.size() * sizeof(glm::vec4), coordinates.data(), GL_STATIC_DRAW);

    glGenBuffers(1, &resultBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, resultBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, results.size() * sizeof(glm::vec4), nullptr, GL_STATIC_DRAW);

    // 绑定缓冲区到计算着色器
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, matrixBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, coordinateBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, resultBuffer);

    // 执行计算着色器
    glUseProgram(shaderProgram);
    glDispatchCompute(static_cast<GLuint>(matrices.size()), 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT); // 确保结果可用

    // 读取结果
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, resultBuffer);
    glm::vec4* resultPtr = (glm::vec4*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
    if (resultPtr) {
        for (size_t i = 0; i < results.size(); ++i) {
            results[i] = resultPtr[i];
            std::cout << "Result " << i << ": ("
                << results[i].x << ", "
                << results[i].y << ", "
                << results[i].z << ", "
                << results[i].w << ")" << std::endl;
        }
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    }

    // 清理
    glDeleteBuffers(1, &matrixBuffer);
    glDeleteBuffers(1, &coordinateBuffer);
    glDeleteBuffers(1, &resultBuffer);
    glDeleteProgram(shaderProgram);
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
