#ifndef WINTOOL_H
#define WINTOOL_H

#include <string>

class WinTools
{
public:
    // 将文件移动到回收站
    static bool moveToRecycleBin(const std::wstring &filePath);
};

#endif // !WINTOOL_H