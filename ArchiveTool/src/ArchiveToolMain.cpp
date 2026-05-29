#include <iostream>
#include <string>

#include <bit7z/bitfileextractor.hpp>

#include "Global.h"

extern "C"
{
    __declspec(dllexport) void say_hello()
    {
        std::cout << "Hello from MyLibrary!" << std::endl;
    }

    // 显式初始化接口：由 C# 在后台线程调用，避免 DllMain 阻塞
    ZYB_ARCHIVE_TOOL_API void InitArchiveTool()
    {
        ::Get7zLibrary();  // 触发 7-Zip 库加载和初始化
    }
}

// DLL 加载时调用
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        // 不在 DllMain 中初始化 7-Zip（耗时操作），改由 InitArchiveTool() 显式调用
        break;
    case DLL_PROCESS_DETACH:
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    }
    return TRUE;
}
