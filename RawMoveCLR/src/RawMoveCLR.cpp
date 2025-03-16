#include <msclr/marshal_cppstd.h>

#include <iostream>
#include <string>
#include <vector>

#using < System.dll>
using namespace System;
using namespace System::Collections::Generic;
using namespace System::Text;

#include "RawMoveMain.h"

namespace NS_RawMoveCLR
{
public ref class RawMoveCLR
{
public:
    static void test()
    {
        say_hello();
    }
    static String^ GetManagedString(const std::string &str) {
        // 使用 GBK 编码
        String ^ managedStr = gcnew String(str.c_str());
        array<Byte> ^ byteArray = Encoding::Default->GetBytes(managedStr); // 转换为字节数组
        String ^ managedString = Encoding::Default->GetString(byteArray); // 将字节数组转换为托管字符串
        return managedString;
    }

    static std::wstring GetStdWString(String ^ cSharString) {
        // 将 String^ 转为本地编码的 std::string
        using namespace System::Runtime::InteropServices;

        // 获取 UTF-16 的 wchar_t* 指针
        const wchar_t *wcharStr =
            reinterpret_cast<const wchar_t *>(Marshal::StringToHGlobalUni(cSharString).ToPointer());

        // 转换为本地编码
        std::wstring wideStr(wcharStr);
        Marshal::FreeHGlobal(IntPtr((void *)wcharStr));

        return wideStr;
    }
};
}  // namespace NS_RawMoveCLR
