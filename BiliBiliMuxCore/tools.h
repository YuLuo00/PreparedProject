#include <string>
#include <fstream>
#include <string>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>
nlohmann::json;


namespace Tools
{
    std::string AvErrorCode2Str(int errCode);
    bool StringFromFile(const std::string &file, std::string &cotent);

    bool TextFromFile(const std::string &path, std::string &outText, std::string *errMsg = nullptr);
    std::string TextFromFile(const std::string &path);
    bool ParseJsonSafe(const std::string &s, nlohmann::json &out, std::string *err = nullptr);

    // 返回 true 表示文件是合法的 UTF-8（允许可选 BOM）
    bool IsUtf8File(const std::string &path, bool allow_bom = true);

    }





