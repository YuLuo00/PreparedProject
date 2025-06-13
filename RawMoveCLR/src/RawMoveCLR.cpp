#include <msclr/marshal_cppstd.h>

#include <functional>
#include <iostream>
#include <string>
#include <vector>

#using < System.dll>
using namespace System;
using namespace System::Collections::Generic;
using namespace System::Text;
using namespace System;
using namespace System::Runtime::InteropServices;

#include "Msg2UI.h"
#include "RawMoveMain.h"

namespace NS_RawMoveCLR
{
public
ref class RawMoveCLR
{
public:
    RawMoveCLR()
    {
        m_core = new RawMoveCore();
    }
    delegate void CallbackDelegate(IntPtr value, IntPtr ptr, int level);
    void SetCbErrMsg(CallbackDelegate ^ cb)
    {
        if (cb == nullptr) {
            m_core->m_msg2ui.SetCallBackWrnMsg(nullptr);
            return;
        }
        IntPtr funcPtr = Marshal::GetFunctionPointerForDelegate(cb);
        m_core->m_msg2ui.SetCallBackErrMsg(static_cast<MsgCBFuncPtr>(funcPtr.ToPointer()));
    }

    ~RawMoveCLR()
    {
        if (this->m_core != nullptr) {
            delete this->m_core;
            this->m_core = nullptr;
        }
    }
    // 定义一个委托类型

    //private:
    RawMoveCore *m_core = nullptr;
    void SetRootDir(String ^ rootDir)
    {
        m_core->m_rootDirPath = GetStdWString(rootDir);
    }
    void SetRawStoreDir(String ^ rawStoreDir)
    {
        m_core->m_RawsStorePath = GetStdWString(rawStoreDir);
    }

    bool MoveAllRawToStorePath()
    {
        return m_core->MoveAllRawToStorePath();
    }

public:
    static void test()
    {
        RawMoveCore rawMoveCore;
        int test = rawMoveCore.GetNum();
        say_hello();
    }
    static String ^
        GetManagedString(const std::string &str) {
            // 使用 GBK 编码
            String ^ managedStr = gcnew String(str.c_str());
            array<Byte> ^ byteArray = Encoding::Default->GetBytes(managedStr); // 转换为字节数组
            String ^ managedString = Encoding::Default->GetString(byteArray); // 将字节数组转换为托管字符串
            return managedString;
        }

    static std::wstring GetStdWString(String ^ cSharString)
    {
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
} // namespace NS_RawMoveCLR
