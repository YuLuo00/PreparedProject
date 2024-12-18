#ifndef _GL_GEOMETRY_H
#define _GL_GEOMETRY_H

// 使用 #pragma pack(1) 禁止内存对齐
#pragma pack(push, 1)
struct VertexInfo
{
public:
    VertexInfo() = default;
    glm::vec2 m_textCoord = {};
    glm::vec3 position = {};
    glm::vec4 color = {};
    glm::vec2 uv = {};
    static const int LengthByFloat;
};
#pragma pack(pop)

inline const int VertexInfo::LengthByFloat = sizeof(VertexInfo) / sizeof(float);

const int GlVertexSize = sizeof(VertexInfo);
const int GlVertexLength = sizeof(VertexInfo) / sizeof(float);

#endif // !_GL_GEOMETRY_H
