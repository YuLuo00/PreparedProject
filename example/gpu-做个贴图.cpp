#include <iostream>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <stb_image.h>

// 顶点数据：位置和纹理坐标
GLfloat vertices[] = {
    // 位置              // 纹理坐标
    -0.0f,  4.0f, 0.0f,  0.0f, 1.0f,  // 左上
    -0.0f,  0.0f, 0.0f,  0.0f, 0.0f,  // 左下
     5.0f,  5.0f, 0.0f,  1.0f, 1.0f,  // 右上

     0.0f,  0.0f, 0.0f,  0.0f, 0.0f,  // 左下
    2.7f,  0.0f, 0.0f,  1.0f, 0.0f,  // 右下
    2.7f, 4.0f, 0.0f,  1.0f, 1.0f   // 右上
};


// 顶点着色器代码
const char* vertexShaderSource = R"(
#version 330 core
layout(location = 0) in vec3 position;  // 顶点位置
layout(location = 1) in vec2 texCoord;  // 纹理坐标

out vec2 TexCoord;  // 传递给片段着色器的纹理坐标

void main()
{
    gl_Position = vec4(position, 1.0f);  // 设置顶点位置
    TexCoord = texCoord;                // 将纹理坐标传递到片段着色器
}
)";

// 片段着色器代码
const char* fragmentShaderSource = R"(
#version 330 core
in vec2 TexCoord;  // 来自顶点着色器的纹理坐标

out vec4 color;    // 输出颜色

uniform sampler2D texture1;  // 纹理采样器

void main()
{
    // 翻转 Y 坐标，使纹理不倒置
    color = texture(texture1, vec2(TexCoord.x, 1.0 - TexCoord.y));
}

)";

// 编译着色器的函数
GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    // 检查编译错误
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "Shader Compilation Error: " << infoLog << std::endl;
    }

    return shader;
}

// 创建着色器程序的函数
GLuint createShaderProgram() {
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    // 检查链接错误
    GLint success;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
        std::cerr << "Program Linking Error: " << infoLog << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}

// 加载纹理的函数
GLuint loadTexture(const char* path) {
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    int width, height, nrChannels;
    unsigned char* data = stbi_load(path, &width, &height, &nrChannels, 0);
    if (data) {
        // 如果图片是 RGB 格式（没有 alpha 通道），使用 GL_RGB
        if (nrChannels == 3) {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
        }
        // 如果图片有 alpha 通道（RGBA），使用 GL_RGBA
        else if (nrChannels == 4) {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        }
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    else {
        std::cout << "Failed to load texture" << std::endl;
    }
    stbi_image_free(data);

    return texture;
}

int main() {
    // 初始化GLFW
    if (!glfwInit()) {
        std::cerr << "GLFW Initialization Failed!" << std::endl;
        return -1;
    }

    // 创建窗口
    GLFWwindow* window = glfwCreateWindow(400, 400, "OpenGL Texture Mapping", nullptr, nullptr);
    if (!window) {
        std::cerr << "GLFW Window Creation Failed!" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, [](GLFWwindow* window, int width, int height) {
        glViewport(0, 0, width, height);
        });

    // 初始化GLEW
    if (glewInit() != GLEW_OK) {
        std::cerr << "GLEW Initialization Failed!" << std::endl;
        return -1;
    }

    // 创建着色器程序
    GLuint shaderProgram = createShaderProgram();

    // 创建VBO和VAO
    GLuint VBO, VAO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // 设置顶点位置属性
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(0);

    // 设置纹理坐标属性
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0); // 解绑 VAO

    // 加载纹理
    GLuint texture = loadTexture(R"(C:\_File\miko酱ww No.049-琳妮特 [36P-997MB]\(6).png)");

    // 使用着色器程序
    glUseProgram(shaderProgram);

    // 绑定纹理
    //glBindTexture(GL_TEXTURE_2D, texture);
    // 渲染循环
    while (!glfwWindowShouldClose(window)) {
        // 处理输入
        glfwPollEvents();

        // 清空屏幕
        glClear(GL_COLOR_BUFFER_BIT);

        // 绘制矩形（由两个三角形组成）
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);

        // 交换缓冲区
        glfwSwapBuffers(window);
    }

    // 清理资源
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteTextures(1, &texture);

    glfwTerminate();
    return 0;
}
