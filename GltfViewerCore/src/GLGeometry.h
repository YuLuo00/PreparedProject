#ifndef _GL_GEOMETRY_H
#define _GL_GEOMETRY_H

#include <vector>

#include <glm/fwd.hpp>
#include <glm/glm.hpp>

// 使用 #pragma pack(1) 禁止内存对齐
#pragma pack(push, 1)
struct VertexInfo
{
public:
    VertexInfo() = default;
    glm::vec3 position = {};
    glm::vec4 color = {};
    glm::vec2 uv = {};
    float textrueId = 1;
};

struct InstanceInfo
{
public:
    InstanceInfo() = default;
    glm::mat4 mtx = glm::identity<glm::mat4>();
    std::vector<int> vertexInfoIdx;
};
#pragma pack(pop)

const int GlVertexSize = sizeof(VertexInfo);
const int GlVertexLength = sizeof(VertexInfo) / sizeof(float);

#endif // !_GL_GEOMETRY_H
