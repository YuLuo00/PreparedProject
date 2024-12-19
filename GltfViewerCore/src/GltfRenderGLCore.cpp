#include "GltfRenderGLCore.h"

#include <iostream>
#include <map>
#include <string>
#include <vector>

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <opencv2/opencv.hpp>
#include <tiny_gltf.h>

#include "GLGeometry.h"
#include "GltfRender.h"

// 顶点着色器源码
static const char *vertexShaderSource = R"(
#version 330 compatibility
layout(location = 0) in vec3 position;  // 输入顶点位置
layout(location = 1) in vec2 uv;        // 输入纹理坐标
layout(location = 2) in float textureID;  // 纹理ID

out vec2 fragUV;  // 将纹理坐标传递到片段着色器
flat out float fragTextureID;  // 传递纹理ID到片段着色器

uniform mat4 view;        // 视图矩阵
uniform mat4 projection;  // 投影矩阵

void main() {
    gl_Position = projection * view * vec4(position, 1.0);  // 计算顶点位置
    fragUV = uv;  // 传递纹理坐标到片段着色器
    fragTextureID = textureID;  // 传递纹理ID
}
)";

// 片段着色器源码
static const char *fragmentShaderSource = R"(
#version 330 compatibility
in vec2 fragUV;  // 接收从顶点着色器传递的纹理坐标
flat in float fragTextureID;   // 从顶点着色器传递的纹理ID

out vec4 FragColor;

uniform sampler2D texture0;  // 纹理1
uniform sampler2D texture1;  // 纹理2
//uniform sampler2D textures[32];
uniform sampler2DArray TextureArray;

void main() {
    int tId = int(fragTextureID);

    //if (fragTextureID == 0.0) {
    //    FragColor = texture(texture0, fragUV);  // 采样纹理1
    //} else if (fragTextureID == 1.0) {
    //    FragColor = texture(texture1, fragUV);  // 采样纹理2
    //}
    FragColor = texture(TextureArray, vec3(fragUV, tId));//纹理坐标第三位为纹理序号索引
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
    static GLint worldLoc = -1;
    static bool initFlag = false;
    if (initFlag == false) {
        initFlag = true;
        // 获取 view 和 projection uniform 变量的位置
        viewLoc = glGetUniformLocation(this->shaderProgram, "view");
        projectionLoc = glGetUniformLocation(this->shaderProgram, "projection");
        worldLoc = glGetUniformLocation(this->shaderProgram, "worldMtx");

        // 将单位矩阵传递给着色器中的 uniform 变量
        // 相机的设置
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(this->view));             // 设置 view 矩阵
        glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(this->projection)); // 设置 projection 矩阵
        {
            std::cout << "-------------------------------------------" << std::endl;
            const char *version = (const char *)glGetString(GL_VERSION);
            std::cout << fmt::format("OpenGL version: {}", version) << std::endl;
            // 获取最大纹理尺寸
            GLint maxTextureSize;
            glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
            std::cout << "Maximum texture size: " << maxTextureSize << std::endl;
            // 最大纹理单元数
            GLint maxTextures;
            glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &maxTextures);
            std::cout << "Maximum texture units: " << maxTextures << std::endl;
            // 最大纹理数组层数
            GLint maxArrayLayers;
            glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &maxArrayLayers);
            std::cout << fmt::format("Maximum texture array layers : {}", maxArrayLayers) << std::endl;
            // 最大绑定点数量
            GLint maxVertexAttribs;
            glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxVertexAttribs);
            std::cout << "Maximum vertex attribute bindings: " << maxVertexAttribs << std::endl;


            // 获取供应商信息
            const GLubyte *vendor = glGetString(GL_VENDOR);
            std::cout << "OpenGL vendor: " << vendor << std::endl;

            // 获取渲染器信息
            const GLubyte *renderer = glGetString(GL_RENDERER);
            std::cout << "OpenGL renderer: " << renderer << std::endl;
            // 渲染器
            const GLubyte* shadingLanguageVersion = glGetString(GL_SHADING_LANGUAGE_VERSION);
            std::cout << "GLSL shader language version: " << shadingLanguageVersion << std::endl;

            // 获取扩展信息
            const GLubyte *extensions = glGetString(GL_EXTENSIONS);
            std::string extensionsStr = extensions != nullptr ? reinterpret_cast<const char*>(extensions) : "";
            //std::cout << fmt::format("OpenGL extensions: {}", extensionsStr) << std::endl;
            std::cout << "-------------------------------------------" << std::endl;
        }
    }

    // 检查按键事件
    if (this->statusNeedRefresh.load() == true) {
        this->statusNeedRefresh.store(false);

        view = glm::lookAt(cameraPos, target, up);                                        // 视图矩阵为单位矩阵
        projection = glm::perspective(glm::radians(fov), aspectRatio, m_near, m_far);     // 投影矩阵为单位矩阵
        glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(this->projection)); // 设置 projection 矩阵
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(this->view));
    }

    // 打印帧率
    {
        // 静态变量保存状态
        static int frameCount = 0;                                        // 累计帧数
        static auto lastTime = std::chrono::high_resolution_clock::now(); // 上次计时的时间点
        // 增加帧计数
        frameCount++;
        // 获取当前时间
        auto currentTime = std::chrono::high_resolution_clock::now();
        long long elapsed = std::chrono::duration_cast<std::chrono::seconds>(currentTime - lastTime).count();
        // 每秒打印一次帧率
        if (elapsed >= 1) {
            // 计算帧率
            float fps = frameCount / static_cast<float>(elapsed);
            std::cout << "FPS: " << fps << std::endl;

            // 重置计时器和帧计数
            lastTime = currentTime;
            frameCount = 0;
            {
                //GLuint textureIDLocation = glGetUniformLocation(shaderProgram, "textureID");
                //int textureIDValue;
                //glGetUniformiv(shaderProgram, textureIDLocation, &textureIDValue);
                //std::cout << "textureID value: " << textureIDValue << std::endl;
                //GLint value;
                //glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_POINTER, &value);
                //std::cout << "VertexAttribPointer location 0 points to: " << value << std::endl;
                //glGetVertexAttribiv(1, GL_VERTEX_ATTRIB_ARRAY_POINTER, &value);
                //std::cout << "VertexAttribPointer location 1 points to: " << value << std::endl;
                //glGetVertexAttribiv(2, GL_VERTEX_ATTRIB_ARRAY_POINTER, &value);
                //std::cout << "VertexAttribPointer location 2 points to: " << value << std::endl;
            }
        }
    }
}

void GltfRenderGLCore::KeyCallbackForGlfw(int key, int scancode, int action, int mods)
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

void GltfRenderGLCore::mouse_pos_callback(double xpos, double ypos)
{
    glm::vec2 pos(xpos, ypos);
    glm::vec2 posMove = pos - this->m_mousePostionLast;
    this->m_mousePostionLast = pos;

    if (this->m_mouseLeftRepeat == true) {
        // 将偏移量转换为旋转角度（调整敏感度）
        float sensitivityX = 0.5; // 灵敏度
        float sensitivityY = 1;   // 灵敏度

        float angleX = static_cast<float>(posMove.y) * sensitivityX * -1; // y 偏移控制绕 x 轴旋转
        float angleY = static_cast<float>(posMove.x) * sensitivityY;      // x 偏移控制绕 y 轴旋转

        this->m_cameraPitch += angleX;
        this->m_cameraYaw += angleY;
        if (this->m_cameraPitch > 89.0)
            this->m_cameraPitch = 89.0;
        if (this->m_cameraPitch < -89.0)
            this->m_cameraPitch = -89.0;

        // 通过俯仰角和偏航角计算新的相机方向
        glm::vec3 direction;
        direction.x = cos(glm::radians(this->m_cameraPitch)) * cos(glm::radians(this->m_cameraYaw));
        direction.y = sin(glm::radians(this->m_cameraPitch));
        direction.z = cos(glm::radians(this->m_cameraPitch)) * sin(glm::radians(this->m_cameraYaw));
        direction = glm::normalize(direction);

        // 更新相机位置
        cameraPos = target - direction * glm::length(cameraPos - target); // 保持相机与目标的距离不变

        this->statusNeedRefresh.store(true);
    }
}

void GltfRenderGLCore::mouse_enter_callback(int entered)
{
    if (entered == GLFW_TRUE) {
        std::cout << "mouse enter." << std::endl;
    }
    else if (entered == GLFW_FALSE) {
        std::cout << "mouse leave" << std::endl;
    }
}

void GltfRenderGLCore::mouse_button_callback(int button, int action, int mods)
{
#define BUTTON_ACTION(BUTTON, ACTION) (BUTTON * 100 + ACTION)

    switch (BUTTON_ACTION(button, action)) {
        case BUTTON_ACTION(GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS):
            this->m_mouseLeftRepeat = true;
            std::cout << "Left mouse button pressed." << std::endl;
            break;
        case BUTTON_ACTION(GLFW_MOUSE_BUTTON_LEFT, GLFW_REPEAT):
            break;
        case BUTTON_ACTION(GLFW_MOUSE_BUTTON_LEFT, GLFW_RELEASE):
            this->m_mouseLeftRepeat = false;
            std::cout << "Left mouse button release." << std::endl;
            break;
        case BUTTON_ACTION(GLFW_MOUSE_BUTTON_RIGHT, GLFW_PRESS):
            std::cout << "Right mouse button pressed." << std::endl;
            break;
        case BUTTON_ACTION(GLFW_MOUSE_BUTTON_RIGHT, GLFW_RELEASE):
            std::cout << "RIGHT mouse button RELEASE." << std::endl;
            break;
        case BUTTON_ACTION(GLFW_MOUSE_BUTTON_4, GLFW_PRESS):
            std::cout << "BUTTON_4 mouse button PRESS." << std::endl;
            break;
        case BUTTON_ACTION(GLFW_MOUSE_BUTTON_4, GLFW_RELEASE):
            std::cout << "BUTTON_4 mouse button RELEASE." << std::endl;
            break;
        case BUTTON_ACTION(GLFW_MOUSE_BUTTON_5, GLFW_PRESS):
            std::cout << "BUTTON_5 mouse button PRESS." << std::endl;
            break;
        case BUTTON_ACTION(GLFW_MOUSE_BUTTON_5, GLFW_RELEASE):
            std::cout << "BUTTON_5 mouse button RELEASE." << std::endl;
            break;
        default:
            break;
    }
}

void GltfRenderGLCore::scroll_callback(double xoffset, double yoffset)
{
    double distanceMoveRaio = yoffset > 0 ? yoffset * (0.5 / 7) : yoffset * (1.0 / 7);
    double scaleRatio = 1.0 - distanceMoveRaio;
    //std::cout << fmt::format(" Scroll : ({},{}); {}", xoffset, yoffset, scaleRatio) << std::endl;

    glm::vec3 ToCamera = this->cameraPos - this->target;
    ToCamera = ToCamera * glm::vec3(scaleRatio, scaleRatio, scaleRatio);
    this->cameraPos = this->target + ToCamera;

    this->statusNeedRefresh.store(true);
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
