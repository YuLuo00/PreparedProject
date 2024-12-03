#version 450
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 0) out vec3 fragColor;

void main() {
    fragColor = inColor; // 将颜色传递给片段着色器
    gl_Position = vec4(inPosition, 1.0);
}
