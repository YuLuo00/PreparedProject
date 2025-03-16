#include <iostream>
#include <string>

#include <Windows.h>


ZYB_RAWMOVE_API void say_hello()
{
    std::cout << "Hello from MyLibrary!" << std::endl;
}

// DLL 加载时调用
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        //std::cout << "Library Loaded!" << std::endl;
        break;
    case DLL_PROCESS_DETACH:
        //std::cout << "Library Unloaded!" << std::endl;
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    }
    return TRUE;
}

