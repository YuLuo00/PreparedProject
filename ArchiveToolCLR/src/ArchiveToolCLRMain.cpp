#include <msclr/marshal_cppstd.h>

#include <string>

#using <System.dll>
using namespace System;

#include "ArchiveTool.h"

namespace NS_ArchiveToolCLR
{
    public ref class ArchiveToolCLR
    {
    public:
        // 定义一个托管方法来调用 C++ 非托管函数
        bool ArchiveExtraTestCLR(String ^file, String ^passwd)
        {
            std::string fileStr = msclr::interop::marshal_as<std::string>(file);
            std::string passwdStr = msclr::interop::marshal_as<std::string>(passwd);

            return ArchiveExtraTest(fileStr, passwdStr);
        }
        bool ArchiveExtraTestCLR(String ^file)
        {
            return ArchiveExtraTestCLR(file, "");
        }
    };
}

