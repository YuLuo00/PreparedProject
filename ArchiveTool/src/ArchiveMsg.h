#include <windows.h>

#include <fstream>
#include <filesystem>
#include <iostream>
#include <map>
#include <string>
namespace fs = std::filesystem;


class ArchiveMsg
{
public:
    static ArchiveMsg &Ins()
    {
        static ArchiveMsg ins;
        return ins;
    }

    void Clear()
    {
        this->msg.clear();
    }

    const std::vector<std::string> &MsgLines()
    {
        return this->msg;
    }


    void AddLine(const std::string &line)
    {
        this->msg.push_back(line);
    }

    void Append(const std::string &line)
    {
        if (this->msg.empty()) {
            this->msg.push_back(line);
        }
        else {
            this->msg.rbegin()->append(line);
        }
    }


private:
    std::vector<std::string> msg;
};
