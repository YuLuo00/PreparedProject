#include <windows.h>

#include <fstream>
#include <filesystem>
#include <iostream>
#include <map>
#include <string>
namespace fs = std::filesystem;

#include <bit7z/bitfileextractor.hpp>

#include <nlohmann/json.hpp>

struct StrCmpIns
{
    bool operator()(const std::string &l, const std::string &r) const volatile
    {
        return stricmp(l.c_str(), r.c_str()) < 0;
    }
};

class ArchiveType
{
public:
    static ArchiveType &Ins()
    {
        static ArchiveType ins;
        static std::once_flag init_flag;
        std::call_once(init_flag, [&]() { ins.Init(); });
        return ins;
    }

    ArchiveType(){};

    const bit7z::BitInFormat *GetFormat(const std::string &typeStr)
    {
        if (this->baseTable.count(typeStr) == 0) {
            return &bit7z::BitFormat::Auto;
        }

        return this->baseTable.at(typeStr);
    }

    void UpdateTable(const std::string &key, const std::string &type)
    {
        this->table[key] = type;
    }

    void Init()
    {
        for (auto iter : this->baseTable) {
            const std::string &key = iter.first;
            this->table[key] = key;
        }

        this->combineWithFile();
        this->dump();
    }

    std::vector<std::string> GetKeys()
    {
        std::vector<std::string> keys;
        for (auto iter : this->table) {
            keys.push_back(iter.first);
        }
        return keys;
    }

    bool combineWithFile()
    {
        std::ifstream file(this->path);
        if (!file.is_open()) {
            return false;
        }
        std::string str((std::istream_iterator<char>(file)), std::istream_iterator<char>());
        file.close();

        try {
            nlohmann::json json = nlohmann::json::parse(str);
            std::map<std::string, std::string, StrCmpIns> fileTable = json;
            std::copy(fileTable.begin(), fileTable.end(), std::inserter(this->table, this->table.end()));
        }
        catch (nlohmann::json::exception &e) {
            return false;
        }

        return true;
    }

    void dump()
    {
        std::ofstream file(this->path, std::ios_base::trunc);

        nlohmann::json json = table;
        std::string jsonStr = json.dump(4);
        file << jsonStr;
        file.close();
    }

private:
    std::string path = "./ArchiveType.json";

    std::map<std::string, std::string, StrCmpIns> table;
    const std::map<std::string, const bit7z::BitInFormat *, StrCmpIns> baseTable{
        {"rar", &bit7z::BitFormat::Rar},
        {"Rar5", &bit7z::BitFormat::Rar5},
        {"BZip2", &bit7z::BitFormat::BZip2},
        {"Zip", &bit7z::BitFormat::Zip},
        {"SevenZip", &bit7z::BitFormat::SevenZip},
        {"Auto", &bit7z::BitFormat::Auto},
    };
};
