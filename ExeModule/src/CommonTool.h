#ifndef _COMMONTOOL_H
#define _COMMONTOOL_H

#include <Windows.h>

#include <string>
#include <vector>

namespace CommonTool
{
std::string Local2Utf8(const std::string &str);
std::string Utf82Local(const std::string &str);

std::string Wstr2Utf8(const std::wstring &wstr);
std::string Wstr2Local(const std::wstring &wstr);

bool StrIsUtf8Bom(const std::string &str);
bool FileIsUtf8Bom(const std::string &filePath);

std::vector<std::string> ReadFileTxtAsLocal(const std::string &filePath);
} // namespace CommonTool

#endif // !_COMMONTOOL_H