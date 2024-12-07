#include <msclr/marshal_cppstd.h>

#include <iostream>
#include <string>
#include <vector>

#using < System.dll>
using namespace System;
using namespace System::Collections::Generic;
using namespace System::Text;

#include "ArchiveTool.h"
#include "PwdManager.h"

namespace NS_ArchiveToolCLR
{
public ref class ArchiveToolCLR
{
public:
    // 定义一个托管方法来调用 C++ 非托管函数
    static bool ArchiveExtraTestCLR( String ^ file, String ^ passwd , String^ type) {
        std::string fileStr   = msclr::interop::marshal_as< std::string >( file );
        std::string passwdStr = msclr::interop::marshal_as<std::string>(passwd);
        std::string typeStr = msclr::interop::marshal_as<std::string>(type);

        return ArchiveExtraTest( fileStr, passwdStr, typeStr );
    }

    static void UpdateTableCLR(String ^ key, String ^ type)
    {
        std::string keyStr = msclr::interop::marshal_as<std::string>(key);
        std::string typeStr = msclr::interop::marshal_as<std::string>(type);
        return UpdateTable(keyStr, typeStr);
    }

    static List<String ^> ^ GetKeysCLR()
    {
        List<String ^> ^ ret = gcnew List<String ^>();

        std::vector<std::string> keys = GetKeys();
        for (size_t i = 0; i < keys.size(); i++) {
            ret->Add(GetManagedString(keys[i]));
        }

        return ret;
    }

    static String ^CheckFormatCLR(String ^ filepath) {
        std::string filePathStr = msclr::interop::marshal_as<std::string>(filepath);
        std::string format = check_format(filePathStr);
        String ^ ret = GetManagedString(format);

        return ret;
    }

    static String^ GetManagedString(const std::string &str) {
        // 使用 GBK 编码
        String ^ managedStr = gcnew String(str.c_str());
        array<Byte> ^ byteArray = Encoding::Default->GetBytes(managedStr); // 转换为字节数组
        String ^ managedString = Encoding::Default->GetString(byteArray); // 将字节数组转换为托管字符串
        return managedString;
    }
};

public ref class PwdManagerCLR
{
public:
    static List<String^>^ GetAllPasswords() {
        // 调用标准 C++ 函数
        std::vector<std::string> passwords = PwdManager::Ins().GetAllPwd();

        // 创建一个托管的 List<String^> 对象
        List<String ^> ^ managedList = gcnew List<String ^>();

        // 将 std::vector<std::string> 转换为 List<String^>
        for (const std::string &pwd : passwords) {
            // 使用 GBK 编码
            String ^ managedPwd = gcnew String(pwd.c_str());
            array<Byte> ^ byteArray = Encoding::Default->GetBytes(managedPwd); // 转换为字节数组
            String ^ managedString = Encoding::Default->GetString(byteArray); // 将字节数组转换为托管字符串
            managedList->Add(managedString);                                  // 添加到 List<String^>
        }

        return managedList;
    }

    static bool AddPws(String ^ pwd)
    {
        std::string pwdStr = msclr::interop::marshal_as< std::string >( pwd );
        return PwdManager::Ins().AddNewPwd( pwdStr );
    }
};
}  // namespace NS_ArchiveToolCLR
