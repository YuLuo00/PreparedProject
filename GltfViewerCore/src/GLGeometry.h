#ifndef _GL_GEOMETRY_H
#define _GL_GEOMETRY_H

// 使用 #pragma pack(1) 禁止内存对齐
#pragma pack(push, 1)
struct VertexInfo
{
public:
    VertexInfo() = default;
    struct Position
    {
        Position() = default;
        Position(float _x, float _y, float _z): x(_x), y(_y), z(_z) { }
        float x = 0;
        float y = 0;
        float z = 0;
    } position;
    //struct Color
    //{
    //    Color() = default;
    //    Color(float _r, float _g, float _b, float _a): r(_r), g(_g), b(_b), a(_a) { }
    //    float r = 0;
    //    float g = 0;
    //    float b = 0;
    //    float a = 0;
    //} color;
    //struct UV
    //{
    //    UV() = default;
    //    UV(float _u, float _v): u(_u), v(_v) { }
    //    float u = 0;
    //    float v = 0;
    //} uv;
};
#pragma pack(pop)

const int GlVertexSize = sizeof(VertexInfo);
const int GlVertexLength = sizeof(VertexInfo) / sizeof(float);

#endif // !_GL_GEOMETRY_H
