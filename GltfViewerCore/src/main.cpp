
#include <iostream>
#include <string>
#include <vector>
#ifndef TINYGLTF_IMPLEMENTATION
    #define TINYGLTF_IMPLEMENTATION
#endif
#ifndef STB_IMAGE_IMPLEMENTATION
    #define STB_IMAGE_IMPLEMENTATION
#endif
#ifndef STB_IMAGE_WRITE_IMPLEMENTATION
    #define STB_IMAGE_WRITE_IMPLEMENTATION
#endif


#include <tiny_gltf.h>


#include "GltfRender.h"

int main()
{
    GltfRender render;

    //render.LoadGltf("Buggy.gltf");
    render.Run();
    return 0;
}
