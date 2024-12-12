#include <msclr/marshal_cppstd.h>

#include <iostream>
#include <string>
#include <vector>

#using < System.dll>
using namespace System;
using namespace System::Collections::Generic;
using namespace System::Text;




namespace NS_GltfViewerCLR
{
public ref class GltfViewerCLR
{
public:
    //// 定义一个托管方法来调用 C++ 非托管函数
    //static bool ArchiveExtraTestCLR( String ^ file, String ^ passwd , String^ type) {
    //    std::string fileStr   = msclr::interop::marshal_as< std::string >( file );
    //    std::string passwdStr = msclr::interop::marshal_as<std::string>(passwd);
    //    std::string typeStr = msclr::interop::marshal_as<std::string>(type);

    //    return ArchiveExtraTest( fileStr, passwdStr, typeStr );
    //}

    //static String ^Msg()
    //{
    //    std::vector<std::string> msgs = ArchiveToolMsg();
    //    String ^ ret = gcnew String("");
    //    for (size_t i = 0; i < msgs.size(); i++) {
    //        ret += GetManagedString(msgs[i]) + Environment::NewLine;
    //    }

    //    return ret;
    //}

    //static void UpdateTableCLR(String ^ key, String ^ type)
    //{
    //    std::string keyStr = msclr::interop::marshal_as<std::string>(key);
    //    std::string typeStr = msclr::interop::marshal_as<std::string>(type);
    //    return UpdateTable(keyStr, typeStr);
    //}

    //static List<String ^> ^ GetKeysCLR()
    //{
    //    List<String ^> ^ ret = gcnew List<String ^>();

    //    std::vector<std::string> keys = GetKeys();
    //    for (size_t i = 0; i < keys.size(); i++) {
    //        ret->Add(GetManagedString(keys[i]));
    //    }

    //    return ret;
    //}

    //static String ^CheckFormatCLR(String ^ filepath) {
    //    std::string filePathStr = msclr::interop::marshal_as<std::string>(filepath);
    //    std::string format = check_format(filePathStr);
    //    String ^ ret = GetManagedString(format);

    //    return ret;
    //}

    static String^ GetManagedString(const std::string &str) {
        // 使用 GBK 编码
        String ^ managedStr = gcnew String(str.c_str());
        array<Byte> ^ byteArray = Encoding::Default->GetBytes(managedStr); // 转换为字节数组
        String ^ managedString = Encoding::Default->GetString(byteArray); // 将字节数组转换为托管字符串
        return managedString;
    }

    //static String^ TryDetermineTypeCLR(String^ filePath)
    //{
    //    std::string filePathStr = msclr::interop::marshal_as<std::string>(filePath);
    //    std::string type = TryDetermineType(filePathStr);
    //    return GetManagedString(type);
    //}
};
}  // namespace NS_ArchiveToolCLR
