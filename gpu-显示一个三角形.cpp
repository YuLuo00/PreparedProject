#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <fstream>
#include <sstream>

// 顶点着色器源代码
const char* vertexShaderSource = R"(
#version 330 core
layout(location = 0) in vec3 position; // 输入顶点位置
layout(location = 1) in vec3 color;    // 输入顶点颜色

out vec3 fragColor; // 输出到片段着色器

void main() {
    gl_Position = vec4(position, 1.0); // 设置顶点位置
    fragColor = color; // 将颜色传递给片段着色器
}
)";

// 几何着色器源代码
const char* geometryShaderSource = R"(
#version 330 core
layout(triangles) in; // 输入为三角形
layout(triangle_strip, max_vertices = 3) out; // 输出为三角形条带

in vec3 fragColor[]; // 从顶点着色器接收颜色
out vec3 geomColor; // 输出到片段着色器

void main() {
    for (int i = 0; i < 3; i++) {
        geomColor = fragColor[i]; // 将颜色传递给片段着色器
        gl_Position = gl_in[i].gl_Position; // 传递顶点位置
        EmitVertex(); // 发出顶点
    }
    EndPrimitive(); // 结束图元
}
)";

// 片段着色器源代码
const char* fragmentShaderSource = R"(
#version 330 core
in vec3 geomColor; // 从几何着色器接收颜色
out vec4 finalColor; // 输出最终颜色

void main() {
    finalColor = vec4(geomColor, 1.0); // 输出颜色
}
)";

// 编译着色器
GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    // 检查编译错误
    GLint isSuccess;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &isSuccess);
    if (!isSuccess) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "Shader compilation error: " << infoLog << std::endl;
    }
    return shader;
}

// 创建着色器程序
GLuint createShaderProgram() {
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint geometryShader = compileShader(GL_GEOMETRY_SHADER, geometryShaderSource);
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);

    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, geometryShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    // 删除着色器，因为它们已经链接到程序中
    glDeleteShader(vertexShader);
    glDeleteShader(geometryShader);
    glDeleteShader(fragmentShader);

    return program;
}

int main() {
    // 初始化 GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    // 创建窗口
    GLFWwindow* window = glfwCreateWindow(800, 600, "OpenGL Triangle", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    // 初始化 GLEW
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW" << std::endl;
        return -1;
    }

    // 定义三角形的顶点数据
    float vertices[] = {
        // 位置           // 颜色
         0.0f,  0.5f, 0.0f,  1.0f, 0.0f, 0.0f,  // 顶点1 (红色)
        -0.5f, -0.5f, 0.0f,  0.0f, 1.0f, 0.0f,  // 顶点2 (绿色)
         0.5f, -0.5f, 0.0f,  0.0f, 0.0f, 1.0f   // 顶点3 (蓝色)
    };

    // 创建 VAO 和 VBO
    GLuint VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // 设置顶点属性指针
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // 创建着色器程序
    GLuint shaderProgram = createShaderProgram();

    // 使用着色器程序
    glUseProgram(shaderProgram);
    // 渲染循环
    while (!glfwWindowShouldClose(window)) {
        // 清屏
        glClear(GL_COLOR_BUFFER_BIT);

        // 绘制三角形
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glBindVertexArray(0);

        // 交换缓冲区和轮询事件
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // 清理资源
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
