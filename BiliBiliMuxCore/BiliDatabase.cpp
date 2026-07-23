#include "BiliDatabase.h"

#include <ctime>

#include <sqlite3.h>

#include "Logger.h"
#include "attach.h"
#include "tools.h"

namespace
{
void SetError(std::string *errMsg, const std::string &message)
{
    if (errMsg) {
        *errMsg = message;
    }
}

std::string PathToUtf8(const std::filesystem::path &path)
{
    return path.u8string();
}

// RAII 封装 sqlite3_stmt，确保异常/早退路径下也会 finalize
class Stmt
{
public:
    Stmt(sqlite3 *db, const char *sql)
    {
        sqlite3_prepare_v2(db, sql, -1, &m_stmt, nullptr);
    }
    ~Stmt()
    {
        if (m_stmt) {
            sqlite3_finalize(m_stmt);
        }
    }
    Stmt(const Stmt &) = delete;
    Stmt &operator=(const Stmt &) = delete;

    sqlite3_stmt *get() const { return m_stmt; }
    explicit operator bool() const { return m_stmt != nullptr; }

    void BindInt64(int index, int64_t value) { sqlite3_bind_int64(m_stmt, index, value); }
    void BindInt(int index, int value) { sqlite3_bind_int(m_stmt, index, value); }
    void BindText(int index, const std::string &value)
    {
        sqlite3_bind_text(m_stmt, index, value.c_str(), -1, SQLITE_TRANSIENT);
    }

    int Step() { return sqlite3_step(m_stmt); }

private:
    sqlite3_stmt *m_stmt = nullptr;
};

std::string WStringToUtf8(const std::wstring &text)
{
    return Tools::wstring_to_utf8(text);
}
} // namespace

BiliDatabase::BiliDatabase(Options options)
    : m_options(std::move(options))
{
}

BiliDatabase::~BiliDatabase()
{
    Close();
}

bool BiliDatabase::Open(std::string *errMsg)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_db != nullptr) {
        return true;
    }

    try {
        if (m_options.create_directories && !m_options.db_path.parent_path().empty()) {
            std::filesystem::create_directories(m_options.db_path.parent_path());
        }
    }
    catch (const std::exception &e) {
        SetError(errMsg, "Failed to create database directory: " + std::string(e.what()));
        return false;
    }

    const std::string pathUtf8 = PathToUtf8(m_options.db_path);
    const int rc = sqlite3_open(pathUtf8.c_str(), &m_db);
    if (rc != SQLITE_OK) {
        SetError(errMsg, "Failed to open database: " + std::string(sqlite3_errmsg(m_db)));
        sqlite3_close(m_db);
        m_db = nullptr;
        return false;
    }

    sqlite3_exec(m_db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);
    sqlite3_exec(m_db, "PRAGMA journal_mode = WAL;", nullptr, nullptr, nullptr);

    if (!EnsureSchema(errMsg)) {
        sqlite3_close(m_db);
        m_db = nullptr;
        return false;
    }

    LOG_INFO(LogGroup::IO, "BiliDatabase opened: {}", pathUtf8);
    return true;
}

void BiliDatabase::Close()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
    }
}

bool BiliDatabase::IsOpen() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_db != nullptr;
}

bool BiliDatabase::EnsureSchema(std::string *errMsg)
{
    static const char *kCreateSourceTable = R"SQL(
        CREATE TABLE IF NOT EXISTS source_media (
            id           INTEGER PRIMARY KEY AUTOINCREMENT,
            avid         INTEGER NOT NULL,
            cid          INTEGER NOT NULL,
            bvid         TEXT,
            page         INTEGER,
            owner_id     INTEGER,
            owner_name   TEXT,
            title        TEXT,
            part_title   TEXT,
            cover_url    TEXT,
            video_md5    TEXT,
            audio_md5    TEXT,
            source_dir   TEXT,
            first_seen_at INTEGER NOT NULL,
            last_seen_at  INTEGER NOT NULL,
            UNIQUE(avid, cid)
        );
    )SQL";

    static const char *kCreateTranscodeTable = R"SQL(
        CREATE TABLE IF NOT EXISTS transcoded_output (
            id           INTEGER PRIMARY KEY AUTOINCREMENT,
            avid         INTEGER NOT NULL,
            cid          INTEGER NOT NULL,
            output_path  TEXT NOT NULL,
            success      INTEGER NOT NULL,
            error_message TEXT,
            muxed_at     INTEGER NOT NULL,
            UNIQUE(avid, cid, output_path)
        );
    )SQL";

    static const char *kCreateTranscodeIndex = R"SQL(
        CREATE INDEX IF NOT EXISTS idx_transcoded_output_avid_cid
            ON transcoded_output(avid, cid);
    )SQL";

    char *errText = nullptr;
    for (const char *sql : {kCreateSourceTable, kCreateTranscodeTable, kCreateTranscodeIndex}) {
        const int rc = sqlite3_exec(m_db, sql, nullptr, nullptr, &errText);
        if (rc != SQLITE_OK) {
            SetError(errMsg, "Failed to create schema: " + std::string(errText ? errText : "unknown error"));
            sqlite3_free(errText);
            return false;
        }
    }

    return true;
}

bool BiliDatabase::IsSourceKnown(int64_t avid, int64_t cid) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_db) {
        return false;
    }

    Stmt stmt(m_db, "SELECT 1 FROM source_media WHERE avid = ? AND cid = ? LIMIT 1;");
    if (!stmt) {
        return false;
    }

    stmt.BindInt64(1, avid);
    stmt.BindInt64(2, cid);
    return stmt.Step() == SQLITE_ROW;
}

bool BiliDatabase::IsTranscoded(int64_t avid, int64_t cid) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_db) {
        return false;
    }

    Stmt stmt(m_db,
              "SELECT 1 FROM transcoded_output WHERE avid = ? AND cid = ? AND success = 1 LIMIT 1;");
    if (!stmt) {
        return false;
    }

    stmt.BindInt64(1, avid);
    stmt.BindInt64(2, cid);
    return stmt.Step() == SQLITE_ROW;
}

bool BiliDatabase::UpsertSource(const SourceRecord &record, std::string *errMsg)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_db) {
        SetError(errMsg, "Database is not open.");
        return false;
    }

    static const char *kUpsertSql = R"SQL(
        INSERT INTO source_media
            (avid, cid, bvid, page, owner_id, owner_name, title, part_title,
             cover_url, video_md5, audio_md5, source_dir, first_seen_at, last_seen_at)
        VALUES
            (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT(avid, cid) DO UPDATE SET
            bvid = excluded.bvid,
            page = excluded.page,
            owner_id = excluded.owner_id,
            owner_name = excluded.owner_name,
            title = excluded.title,
            part_title = excluded.part_title,
            cover_url = excluded.cover_url,
            video_md5 = excluded.video_md5,
            audio_md5 = excluded.audio_md5,
            source_dir = excluded.source_dir,
            last_seen_at = excluded.last_seen_at;
    )SQL";

    Stmt stmt(m_db, kUpsertSql);
    if (!stmt) {
        SetError(errMsg, "Failed to prepare UpsertSource statement: " + std::string(sqlite3_errmsg(m_db)));
        return false;
    }

    const int64_t nowUnix = static_cast<int64_t>(std::time(nullptr));

    stmt.BindInt64(1, record.avid);
    stmt.BindInt64(2, record.cid);
    stmt.BindText(3, WStringToUtf8(record.bvid));
    stmt.BindInt(4, record.page);
    stmt.BindInt(5, record.owner_id);
    stmt.BindText(6, WStringToUtf8(record.owner_name));
    stmt.BindText(7, WStringToUtf8(record.title));
    stmt.BindText(8, WStringToUtf8(record.part_title));
    stmt.BindText(9, WStringToUtf8(record.cover_url));
    stmt.BindText(10, WStringToUtf8(record.video_md5));
    stmt.BindText(11, WStringToUtf8(record.audio_md5));
    stmt.BindText(12, WStringToUtf8(record.source_dir));
    stmt.BindInt64(13, nowUnix);
    stmt.BindInt64(14, nowUnix);

    if (stmt.Step() != SQLITE_DONE) {
        SetError(errMsg, "Failed to upsert source_media: " + std::string(sqlite3_errmsg(m_db)));
        return false;
    }

    LOG_DEBUG(LogGroup::IO, "Source recorded: avid={}, cid={}", record.avid, record.cid);
    return true;
}

bool BiliDatabase::RecordTranscodeResult(const TranscodeRecord &record, std::string *errMsg)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_db) {
        SetError(errMsg, "Database is not open.");
        return false;
    }

    static const char *kInsertSql = R"SQL(
        INSERT INTO transcoded_output
            (avid, cid, output_path, success, error_message, muxed_at)
        VALUES
            (?, ?, ?, ?, ?, ?)
        ON CONFLICT(avid, cid, output_path) DO UPDATE SET
            success = excluded.success,
            error_message = excluded.error_message,
            muxed_at = excluded.muxed_at;
    )SQL";

    Stmt stmt(m_db, kInsertSql);
    if (!stmt) {
        SetError(errMsg,
                 "Failed to prepare RecordTranscodeResult statement: " + std::string(sqlite3_errmsg(m_db)));
        return false;
    }

    const int64_t nowUnix = static_cast<int64_t>(std::time(nullptr));

    stmt.BindInt64(1, record.avid);
    stmt.BindInt64(2, record.cid);
    stmt.BindText(3, WStringToUtf8(record.output_path));
    stmt.BindInt(4, record.success ? 1 : 0);
    stmt.BindText(5, WStringToUtf8(record.error_message));
    stmt.BindInt64(6, nowUnix);

    if (stmt.Step() != SQLITE_DONE) {
        SetError(errMsg, "Failed to record transcode result: " + std::string(sqlite3_errmsg(m_db)));
        return false;
    }

    LOG_DEBUG(LogGroup::IO,
              "Transcode result recorded: avid={}, cid={}, success={}",
              record.avid,
              record.cid,
              record.success);
    return true;
}

BiliDatabase::SourceRecord BiliDatabase::SourceRecordFromMediaInfo(const MediaInfo &info,
                                                                   const std::filesystem::path &sourceDir,
                                                                   const std::wstring &partTitle,
                                                                   const std::wstring &videoMd5,
                                                                   const std::wstring &audioMd5)
{
    SourceRecord record;
    record.avid = info.avid;
    record.cid = info.cid;
    record.bvid = info.bvid;
    record.page = info.page;
    record.owner_id = info.owner_id;
    record.title = info.title;
    record.part_title = partTitle;
    record.cover_url = info.cover;
    record.video_md5 = !videoMd5.empty() ? videoMd5 : (info.videos.empty() ? std::wstring{} : info.videos.front().md5);
    record.audio_md5 = !audioMd5.empty() ? audioMd5 : (info.audios.empty() ? std::wstring{} : info.audios.front().md5);
    record.source_dir = sourceDir.wstring();
    return record;
}
