#pragma once
#ifndef GLTFRENDER_H
#define GLTFRENDER_H

#include <string>

class GltfRenderGLCore;
class ZYB_GLTFVIEWER_API GltfRender
{
public:
    GltfRender();

    bool LoadGltf(const std::string &filePath);
    int Run();

    GltfRenderGLCore *m_data = nullptr;

protected:

private:
    bool Init();
};

#endif // !GLTFRENDER_H
