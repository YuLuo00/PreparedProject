
#include <iostream>
#include <string>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_resize.h>





#include <tiny_gltf.h>


#include "GltfRender.h"

int main()
{
    GltfRender render;

    //render.LoadGltf("Buggy.gltf");
    render.Run();
    return 0;
}
