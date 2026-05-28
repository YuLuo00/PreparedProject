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
}

// DLL 加载时调用
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        Get7zLibrary();
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

void main()
{
    try
    { // bit7z classes can throw BitException objects
        using namespace bit7z;

        Bit7zLibrary lib{"7zip.dll"};
        BitFileExtractor extractor{lib, BitFormat::Auto};

        // Extracting a simple archive
        extractor.setPassword("test");
        extractor.extract("bin.rar", "out/dir/");
        //extractor.test("bin.rar");

        //// Extracting an encrypted archive
        //extractor.setPassword("test");
        //extractor.extract("path/to/another/archive.7z", "out/dir/");
    }
    catch (const bit7z::BitException &ex)
    {
        std::cout << ex.what() << std::endl;
        exit(-1);
    }
}
