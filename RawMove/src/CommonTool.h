#ifndef COMMON_TOOL_H
#define COMMON_TOOL_H

#include <Windows.h>

#include <functional>
#include <string>
#include <vector>
#include <cwctype> // for std::towlower

namespace CommonTool
{
std::string Local2Utf8(const std::string &str);
std::string Utf82Local(const std::string &str);

std::string Wstr2Utf8(const std::wstring &wstr);
std::string Wstr2Local(const std::wstring &wstr);

std::wstring Utf82Wstr(const std::string &str);
std::wstring Local2Wstr(const std::string &str);

bool StrIsUtf8Bom(const std::string &str);
bool FileIsUtf8Bom(const std::string &filePath);

std::vector<std::string> ReadFileTxtAsLocal(const std::string &filePath);

void getFilesInDirectory(const std::string &directory);
} // namespace CommonTool

// 自定义比较器（忽略大小写）
struct CaseInsCmp
{
    bool operator()(const std::string &a, const std::string &b) const
    {
        return std::lexicographical_compare(
            a.begin(), a.end(), b.begin(), b.end(), [](unsigned char c1, unsigned char c2) {
                return std::tolower(c1) < std::tolower(c2);
            });
    }
};
struct CaseInsCmpW
{
    bool operator()(const std::wstring &a, const std::wstring &b) const
    {
        bool ret = std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(), [](wchar_t c1, wchar_t c2) {
            return std::towlower(c1) < std::towlower(c2);
        });

        return ret;
    }

    static bool sameIns(const std::wstring &a, const std::wstring &b)
    {
        static CaseInsCmpW inst;
        bool notSameIns = inst(a, b);
        return !notSameIns;
    }
};

enum class GoOnFind
{
    CONTINUE = 1,
    SKIP = 0,
    BREAK = -1
};

void FindFilesDfs(const std::wstring &directory,
                  std::function<GoOnFind(const std::wstring &folderPath, const WIN32_FIND_DATAW &item)> itemCallback,
                  std::function<void(bool inInto, const std::wstring &folder)> folderInOutCallback);

class CallByRAII
{
public:
    CallByRAII(std::function<void()> cb)
    {
        this->m_cb = cb;
    }
    ~CallByRAII()
    {
        if (this->m_cb) {
            this->m_cb();
        }
    }

private:
    std::function<void()> m_cb;
};

#endif // !COMMON_TOOL_H