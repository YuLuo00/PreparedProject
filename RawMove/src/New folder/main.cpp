#include <iostream>
#include <vector>

#include "GltfRender.h"
#include  "RawMoveTask.h"

#include  <fmt/xchar.h>

int main()
{

    std::wstring name = L"世界";
    fmt::format(L"你好，{}！\n", name);

    RawMoveTask rawMoveTask;
    return 0;
}
