#pragma once

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>

struct sqlite3;
struct MediaInfo;

// 独立的收录/查重数据库模块。
// 只负责“原始素材是否已收录” / “是否已转码成功”的记录与查询，
// 不关心调用方在什么阶段触发写入（扫描时、混流前、混流后均可调用）。
//
// 查重键为 (avid, cid)：cid 由 B 站按“具体上传的一次内容”分配，
// 作者重新编辑并重新上传会产生新的 cid，因此同一 avid 下的新旧版本
// 天然不会被判定为重复，两个版本都会各自收录一条记录。
class BiliDatabase
{
public:
    struct Options
    {
        std::filesystem::path db_path = std::filesystem::path("cache/bilibili.db");
        bool create_directories = true;
    };

    // 对应 source_media 表的一条原始素材记录
    struct SourceRecord
    {
        int64_t avid = 0;
        int64_t cid = 0;
        std::wstring bvid;
        int page = 0;
        int owner_id = 0;
        std::wstring owner_name;
        std::wstring title;
        std::wstring part_title;
        std::wstring cover_url;
        std::wstring video_md5; // index.json 中视频分片的 md5，用于兜底校验
        std::wstring audio_md5; // index.json 中音频分片的 md5
        std::wstring source_dir; // 该素材缓存目录在磁盘上的路径
    };

    // 对应 transcoded_output 表的一条转码结果记录。
    // 同一 (avid, cid) 允许有多条记录（不同 output_path），历史转码不会被覆盖。
    struct TranscodeRecord
    {
        int64_t avid = 0;
        int64_t cid = 0;
        std::wstring output_path;
        bool success = true;
        std::wstring error_message;
    };

    BiliDatabase() = default;
    explicit BiliDatabase(Options options);
    ~BiliDatabase();

    BiliDatabase(const BiliDatabase &) = delete;
    BiliDatabase &operator=(const BiliDatabase &) = delete;

    bool Open(std::string *errMsg = nullptr);
    void Close();
    bool IsOpen() const;

    // 该 (avid, cid) 是否已经收录过原始素材
    bool IsSourceKnown(int64_t avid, int64_t cid) const;

    // 该 (avid, cid) 是否已经存在至少一条成功的转码记录
    bool IsTranscoded(int64_t avid, int64_t cid) const;

    // 收录/更新原始素材记录：按 (avid, cid) upsert，已存在则更新可变字段与 last_seen_at
    bool UpsertSource(const SourceRecord &record, std::string *errMsg = nullptr);

    // 追加一条转码结果记录（不覆盖历史记录）
    bool RecordTranscodeResult(const TranscodeRecord &record, std::string *errMsg = nullptr);

    // 便捷方法：从已解析的 MediaInfo + 素材目录构造 SourceRecord，
    // videoMd5/audioMd5 留空时自动取 info.videos/info.audios 的第一条 md5。
    static SourceRecord SourceRecordFromMediaInfo(const MediaInfo &info,
                                                  const std::filesystem::path &sourceDir,
                                                  const std::wstring &partTitle = {},
                                                  const std::wstring &videoMd5 = {},
                                                  const std::wstring &audioMd5 = {});

private:
    bool EnsureSchema(std::string *errMsg);

    Options m_options;
    sqlite3 *m_db = nullptr;
    mutable std::mutex m_mutex;
};
