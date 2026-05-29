#include "PwdManager.h"

#include <fstream>
#include <filesystem>
namespace fs = std::filesystem;

#include "CommonTool.h"

void PwdManager::Init()
{
    m_pwdBoolPath = fs::absolute(m_pwdBoolPath).generic_string();
    if (!fs::exists(m_pwdBoolPath)) return;

    std::vector<std::string> lines = CommonTool::ReadFileTxtAsLocal(this->m_pwdBoolPath);
    for (const auto &line : lines) {
        if (!line.empty()) m_pwdBook.push_back(line);
    }
}

void PwdManager::SaveToFile()
{
    std::ofstream file(this->m_pwdBoolPath, std::ios_base::trunc);
    if (!file.is_open()) return;
    for (const std::string &pwd : this->m_pwdBook) {
        file << pwd << '\n';
    }
    file.close();
}

bool PwdManager::AddNewPwd(const std::string &pwd)
{
    // 检查是否已存在
    auto it = std::find(m_pwdBook.begin(), m_pwdBook.end(), pwd);
    if (it != m_pwdBook.end()) return true;

    m_pwdBook.push_back(pwd);
    SaveToFile();
    return true;
}

std::vector<std::string> PwdManager::GetAllPwd()
{
    return m_pwdBook;
}

bool PwdManager::PromotePwd(const std::string &pwd)
{
    auto it = std::find(m_pwdBook.begin(), m_pwdBook.end(), pwd);
    if (it == m_pwdBook.end()) return false;  // 不存在
    if (it == m_pwdBook.begin()) return true;  // 已经在最前面

    // 移到最前面
    m_pwdBook.erase(it);
    m_pwdBook.insert(m_pwdBook.begin(), pwd);
    SaveToFile();
    return true;
}
