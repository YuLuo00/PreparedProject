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



#include <iostream>
#include <filesystem>
#include <set>
#include <string>

namespace fs = std::filesystem;

void moveFilesWithPrefixes(const std::set<std::string> &prefixes, const fs::path &srcDir, const fs::path &dstDir)
{
    if (!fs::exists(srcDir) || !fs::is_directory(srcDir)) {
        std::cerr << "源文件夹不存在或不是目录: " << srcDir << "\n";
        return;
    }

    // 创建目标文件夹
    if (!fs::exists(dstDir)) {
        fs::create_directories(dstDir);
    }

    for (const auto &entry : fs::recursive_directory_iterator(srcDir)) {
        if (entry.is_regular_file()) {
            std::string filename = entry.path().filename().string();

            // 检查是否匹配任意前缀
            bool match = false;
            for (const auto &prefix : prefixes) {
                if (filename.rfind(prefix, 0) == 0) { // 0 表示匹配开头
                    match = true;
                    break;
                }
            }

            if (match) {
                fs::path targetPath = dstDir / filename;

                // 如果目标已存在，避免覆盖
                int counter = 1;
                while (fs::exists(targetPath)) {
                    targetPath = dstDir / (entry.path().stem().string() + "_" + std::to_string(counter) +
                                           entry.path().extension().string());
                    counter++;
                }

                try {
                    fs::rename(entry.path(), targetPath);
                    std::cout << "已移动: " << entry.path() << " -> " << targetPath << "\n";
                }
                catch (const fs::filesystem_error &e) {
                    std::cerr << "移动失败: " << e.what() << "\n";
                }
            }
        }
    }
}
