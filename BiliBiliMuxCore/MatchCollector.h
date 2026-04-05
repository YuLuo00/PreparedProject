#pragma once

#include <unordered_set>
#include <string>
#include <filesystem>
#include <iostream>



namespace fs = std::filesystem;

#include "BiliCache.h"
#include "Logger.h"

class MatchCollector
{
public:
    explicit MatchCollector(fs::path targetDir)
        : m_targetDir(std::move(targetDir))
    {}

    // 记录一个 subdir 已完成
    void RecordDone(const MediaSubdir* subdir)
    {
        if (!subdir || !subdir->match)
            return;

        const Match* match = subdir->match;

        std::string matchKey = match->dir.generic_u8string();
        std::string subKey   = subdir->dir.generic_u8string();

        std::string fullKey = MakeKey(matchKey, subKey);

        // 1. 插入完成记录（去重）
        m_finished.insert(fullKey);

        // 2. 判断是否全部完成
        if (IsMatchAllDone(match, matchKey))
        {
            MoveMatchDir(match, matchKey);

            // 3. 清理该 match 的所有记录
            CleanupMatch(match, matchKey);
        }
    }

private:
    fs::path m_targetDir;

    // 已完成 subdir（match + subdir 唯一键）
    std::unordered_set<std::string> m_finished;

private:

    //// 路径规范化（避免 D:/ 和 D:\ 混乱）
    //static std::string Normalize(const fs::path& p)
    //{
    //    try
    //    {
    //        return fs::weakly_canonical(p).string();
    //    }
    //    catch (...)
    //    {
    //        return p.lexically_normal().string();
    //    }
    //}

    static std::string MakeKey(const std::string& match, const std::string& sub)
    {
        return match + "|" + sub;
    }

    bool IsMatchAllDone(const Match* match, const std::string& matchKey)
    {
        for (const auto& s : match->media_subdirs)
        {
            std::string subKey = s.dir.generic_u8string();
            std::string key = MakeKey(matchKey, subKey);

            if (m_finished.find(key) == m_finished.end())
                return false;
        }
        return true;
    }

    void MoveMatchDir(const Match* match, const std::string& matchKey)
    {
        try
        {
            fs::path src = match->dir;
            fs::path dst = m_targetDir / src.filename();

            // 如果目标已存在，可以选择覆盖/跳过
            if (fs::exists(dst))
            {
                LOG_INFO(LogGroup::IO, "Target already exists: {} \n not move {}",
                    dst.generic_u8string(),
                    src.generic_u8string());
                return;
            }

            // 优先 rename（同盘快）
            std::error_code ec;
            fs::rename(src, dst, ec);

            if (ec)
            {
                // 跨盘 fallback
                fs::copy(src, dst, fs::copy_options::recursive);
                fs::remove_all(src);
            }

            LOG_INFO(LogGroup::IO, "Moved: {}  -> {}", src.generic_u8string(), dst.generic_u8string());
        }
        catch (const std::exception& e)
        {
            LOG_ERROR(LogGroup::IO, "Move failed: {}", e.what());
        }
    }

    void CleanupMatch(const Match* match, const std::string& matchKey)
    {
        for (const auto& s : match->media_subdirs)
        {
            std::string subKey = s.dir.generic_u8string();
            m_finished.erase(MakeKey(matchKey, subKey));
        }
    }
};