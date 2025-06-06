//
// Created by yubizhan on 6/6/2025.
//

#ifndef RAWMOVETASK_H
#define RAWMOVETASK_H

#include <map>
#include <set>
#include <string>
#include <filesystem>

#include <fmt/format.h>
#include <fmt/xchar.h>

namespace fs = std::filesystem;

// 比较器：忽略大小写的 wstring 比较
struct CaseInsensitiveWStringLess {
    bool operator()(const std::wstring& lhs, const std::wstring& rhs) const {
        size_t len = std::min(lhs.size(), rhs.size());
        for (size_t i = 0; i < len; ++i) {
            wchar_t lc1 = std::towlower(lhs[i]);
            wchar_t lc2 = std::towlower(rhs[i]);
            if (lc1 < lc2) return true;
            if (lc1 > lc2) return false;
        }
        return lhs.size() < rhs.size();
    }
};


class RawMoveTask
{
public:
    RawMoveTask();
    ~RawMoveTask();

    enum class MsgLevel
    {
        NORMAL,
        WARN,
        ERROR,
    };


    void (*m_OnFinished)() = nullptr;
    void OnFinished() const
    {
        if (m_OnFinished) {
            m_OnFinished();
        }
    }

    void (*m_OnMsg)(const int level, const wchar_t* msg) = nullptr;
    void OnMsg(const MsgLevel level, const std::wstring &msg) const
    {
        if (m_OnMsg) {
            m_OnMsg(static_cast<int>(level), msg.c_str());
        }
    }

    [[nodiscard]]
    bool Scan() const
    {
        if (fs::exists(m_rootDir) == false) {
            return false;
        }

        return true;
    }

    [[nodiscard]]
    bool MoveRaws2StoreDir()
    {
        if (fs::exists(m_RawStoreDir) == false) {
            return false;
        }

        OnMsg(MsgLevel::NORMAL, fmt::format(L"开始移动raw全部raw文件到目标目录 {}", m_RawStoreDir));
        for (const auto &iter : m_raw2Path) {
            const std::wstring &fileName = iter.first;
            const std::set<fs::path> &rawFiles = iter.second;

            OnMsg(MsgLevel::NORMAL, fmt::format(L"开始处理文件 {}", fileName));
            if (rawFiles.size() != 1) {
                OnMsg(MsgLevel::WARN, fmt::format(L"文件数量异常[{}] * {},", fileName, rawFiles.size()));
                m_rawMoveRet.m_filesSkippedForRepeat.insert(rawFiles.begin(), rawFiles.end());
                continue;
            }
            const fs::path &rawFile = *rawFiles.begin();
            fs::rename(rawFile.string(), fs::path(m_RawStoreDir) / rawFile.filename());
            OnMsg(MsgLevel::NORMAL, fmt::format(L"文件移动完成 {}", rawFile.wstring()));
            m_rawMoveRet.m_filesMoved.insert(rawFile);
        }
        OnMsg(MsgLevel::NORMAL, fmt::format(L"处理完成"));
        OnFinished();
        return true;
    }

    [[nodiscard]]
    bool MoveRaw2JpgByFileName(const  std::wstring &jpgFileName) const
    {
        std::wstring fileName = fs::path(jpgFileName).filename().wstring();
        if (m_jpg2Path.count(fileName) == 0) {
            OnMsg(MsgLevel::ERROR, fmt::format(L"移动失败，扫描记录中无jpg文件[{}]", fileName));
            return false;
        }
        std::set<fs::path> rawFiles = m_raw2Path.at(fileName);
        if (rawFiles.empty()) {
            OnMsg(MsgLevel::ERROR, fmt::format(L"移动失败，未匹配到raw文件{}\n{}", fileName));
            return false;
        }
        if (rawFiles.size() > 1) {
            OnMsg(MsgLevel::ERROR, fmt::format(L"移动失败，匹配到多个raw文件{}\n{}", fileName, rawFiles));
            return false;
        }

        std::wstring fullName = rawFiles.begin()->wstring();

        return MoveRaw2JpgByFullPath(fullName);
    }

    [[nodiscard]]
    bool MoveRaw2JpgByFullPath(const std::wstring &jpg) const
    {
        std::wstring fileName = fs::path(jpg).filename().wstring();
        if (m_raw2Path.count(fileName) == 0) {
            OnMsg(MsgLevel::ERROR, fmt::format(L"移动失败，扫描记录中无raw文件[{}]", fileName));
            return false;
        }



        return true;
    }

    struct RawMoveRet
    {
        std::set<fs::path> m_filesSkippedForRepeat;
        std::set<fs::path> m_filesMoved;
    } m_rawMoveRet;

    std::wstring m_rootDir;
    std::wstring m_RawStoreDir;
    std::map<std::wstring, std::set<fs::path>, CaseInsensitiveWStringLess> m_jpg2Path;
    std::map<std::wstring, std::set<fs::path>, CaseInsensitiveWStringLess> m_raw2Path;
};


#endif //RAWMOVETASK_H
