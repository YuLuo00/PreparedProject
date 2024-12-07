#include "PwdManager.h"

#include <fstream>
#include <filesystem>
namespace fs = std::filesystem;

#include "CommonTool.h"

void PwdManager::Init()
{
    m_pwdBoolPath = fs::absolute(m_pwdBoolPath).generic_string();
    if (!fs::exists(m_pwdBoolPath)) {
        return;
    }

    std::vector<std::string> lines = CommonTool::ReadFileTxtAsLocal(this->m_pwdBoolPath);
    m_pwdBook.insert(lines.begin(), lines.end());
    return;
}

bool PwdManager::AddNewPwd(const std::string &pwd)
{
    if (this->m_pwdBook.count(pwd) > 0) {
        return true;
    }

    this->m_pwdBook.insert(pwd);
    std::ofstream file(this->m_pwdBoolPath, std::ios_base::trunc);
    if (!file.is_open()) {
        return false;
    }
    for (const std::string &pwd : this->m_pwdBook) {
        file << pwd << '\n';
    }
    file.close();
}

std::vector<std::string> PwdManager::GetAllPwd()
{
    return std::vector<std::string>(this->m_pwdBook.begin(), this->m_pwdBook.end());
}