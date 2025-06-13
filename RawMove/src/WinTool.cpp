#include "WinTool.h"

#include <iostream>
#include <shlobj.h>
#include <string>
#include <windows.h>

// 将文件移动到回收站
bool WinTools::moveToRecycleBin(const std::wstring &filePath)
{
    std::wstring pFromBuffer = filePath;
    pFromBuffer.push_back(L'\0'); // 第一个 null terminator
    pFromBuffer.push_back(L'\0'); // 第二个 null terminator

    SHFILEOPSTRUCTW fileOp = {0};
    fileOp.wFunc = FO_DELETE;
    fileOp.pFrom = pFromBuffer.c_str(); // 必须是双 null 结尾
    fileOp.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION;
    fileOp.lpszProgressTitle = L"try move to recycleBin";

    int result = SHFileOperationW(&fileOp);

    if (result == 0) {
        std::wcout << L"文件已成功移动到回收站: " << filePath << std::endl;
        return true;
    }
    else {
        std::wcerr << L"移动文件到回收站失败: " << filePath << L"，错误码: " << result << std::endl;
        return false;
    }
}
