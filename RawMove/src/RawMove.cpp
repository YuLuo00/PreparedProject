#include "CommonTool.h"
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/xchar.h>
#include <iostream>
#include <map>
#include <sqlite3.h>
#include <string>
#include <vector>

// 表示一行文件记录
struct FileRecord
{
    std::wstring file_name;
    std::wstring base_name;
    std::wstring full_path; // 主键
    std::wstring location;
    std::wstring file_type;

    struct MemberPtrLess
    {
        bool operator()(const std::wstring FileRecord::*lhs, const std::wstring FileRecord::*rhs) const
        {
            //void *pl = (void *)lhs;
            // 按成员指针的内存内容字节比较
            return std::memcmp(&lhs, &rhs, sizeof(lhs)) < 0;
        }
    };
    //// 成员指针比较器（按地址值排序）
    //struct MemberPtrLess
    //{
    //    bool operator()(const std::wstring FileRecord::*lhs, const std::wstring FileRecord::*rhs) const
    //    {
    //        return reinterpret_cast<std::uintptr_t>(lhs) < reinterpret_cast<std::uintptr_t>(rhs);
    //    }
    //};

    static std::wstring Filed2DbKey(std::wstring FileRecord::*filed)
    {
        std::wstring ret;
        auto iii = &FileRecord::file_name;
        static const std::map<std::wstring FileRecord::*, std::wstring, MemberPtrLess> keyMap = {
            {&FileRecord::file_name, L"file_name"},
            {&FileRecord::base_name, L"base_name"},
            {&FileRecord::full_path, L"full_path"},
            {&FileRecord::location, L"location"},
            {&FileRecord::file_type, L"file_type"}};
        ret = keyMap.at(filed);
        return ret;
    }

    std::wstring toSqlQueryWhere(const std::vector<std::pair<std::wstring FileRecord::*, bool>> fileds)
    {
        std::wstring querySql;
        for (auto iter = fileds.begin(); iter != fileds.end(); iter++) {
            std::wstring FileRecord::*filed = iter->first;
            bool IsAnd = iter->second;

            std::wstring s = fmt::format(LR"({} = "{}")", Filed2DbKey(filed), this->*filed);
            if (iter != fileds.begin()) {
                querySql.append(IsAnd ? L"AND" : L"OR");
            }
            querySql.append(s);
        }
        return querySql;
    }
};

class FileDatabase
{
public:
    FileDatabase();
    ~FileDatabase();

    // 插入或替换一条记录（主键重复时替换）
    bool insertOrReplace(const FileRecord &record);

    void Clear()
    {
        this->execute("DELETE FROM files");
    }

    // 条件查询所有记录（返回满足条件的结果）
    std::vector<FileRecord> query(const std::string &whereClause = "");
    std::vector<FileRecord> query(const std::wstring &whereClause = L"")
    {
        const std::string sqlStr = CommonTool::Wstr2Utf8(whereClause);
        return query(sqlStr);
    }

    std::vector<FileRecord> queryByTypeBasename(const std::wstring &type, const std::wstring &baseName)
    {
        const std::wstring sql = fmt::format(LR"(file_type = "{}" AND base_name = "{}")", type, baseName);
        return query(sql);
    }
    std::vector<FileRecord> queryByType(const std::wstring &type)
    {
        const std::wstring sql = fmt::format(LR"(file_type = "{}")", type);
        return query(sql);
    }

    bool dumpToFile(const std::wstring &filename);
    std::vector<FileRecord> queryRawOnlyNoJpg();
    std::vector<FileRecord> SelectAllBy(const std::wstring sqlAfterSelect)
    {
        std::vector<FileRecord> results;
        std::wstring sqlWstr = L"SELECT file_name, base_name, full_path, location, file_type ";
        sqlWstr += sqlAfterSelect;
        std::string sql = CommonTool::Wstr2Utf8(sqlWstr);

        sqlite3_stmt *stmt = nullptr;
        if (sqlite3_prepare_v2(m_sqliteDb, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            std::cout << "Failed to prepare query.\n";
            return results;
        }

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            FileRecord r;
            r.file_name = reinterpret_cast<const wchar_t *>(sqlite3_column_text16(stmt, 0));
            r.base_name = reinterpret_cast<const wchar_t *>(sqlite3_column_text16(stmt, 1));
            r.full_path = reinterpret_cast<const wchar_t *>(sqlite3_column_text16(stmt, 2));
            r.location = reinterpret_cast<const wchar_t *>(sqlite3_column_text16(stmt, 3));
            r.file_type = reinterpret_cast<const wchar_t *>(sqlite3_column_text16(stmt, 4));

            results.push_back(r);
        }

        sqlite3_finalize(stmt);
        return results;
    }

private:
    sqlite3 *m_sqliteDb;

    bool execute(const std::string &sql);
    bool execute(const std::wstring &sql);
};

#include <iostream>

FileDatabase::FileDatabase()
: m_sqliteDb(nullptr)
{
    if (sqlite3_open(":memory:", &m_sqliteDb) != SQLITE_OK) {
        std::cerr << "Failed to open in-memory SQLite DB\n";
        m_sqliteDb = nullptr;
        return;
    }

    // 创建表，主键为 full_path，所有字段忽略大小写
    const char *create_sql = R"SQL(
        CREATE TABLE files (
            full_path  TEXT PRIMARY KEY COLLATE NOCASE,
            file_name  TEXT COLLATE NOCASE,
            base_name  TEXT COLLATE NOCASE,
            location   TEXT COLLATE NOCASE,
            file_type  TEXT COLLATE NOCASE
        );
    )SQL";
    execute(create_sql);

    // 可选索引（加快多条件筛选）
    execute("CREATE INDEX idx_file_name ON files(file_name);");
    execute("CREATE INDEX idx_base_name ON files(base_name);");
    execute("CREATE INDEX idx_location ON files(location);");
    execute("CREATE INDEX idx_file_type ON files(file_type);");
}

FileDatabase::~FileDatabase()
{
    if (m_sqliteDb)
        sqlite3_close(m_sqliteDb);
}

bool FileDatabase::execute(const std::wstring &sqlWstr)
{
    const std::string str = CommonTool::Wstr2Utf8(sqlWstr);
    const std::wstring wstr = CommonTool::Utf82Wstr(str);
    return execute(str);
}

bool FileDatabase::execute(const std::string &sql)
{
    char *errMsg = nullptr;
    if (sqlite3_exec(m_sqliteDb, sql.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::cerr << "SQLite Error: " << errMsg << "\n";
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

bool FileDatabase::insertOrReplace(const FileRecord &record)
{
    static const wchar_t *sql = L"INSERT OR REPLACE INTO files (full_path, file_name, base_name, location, file_type) "
                                L"VALUES (?, ?, ?, ?, ?);";

    sqlite3_stmt *stmt = nullptr;

    // 准备语句（使用 UTF-16 编码版本）
    if (sqlite3_prepare16_v2(m_sqliteDb, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::wcerr << L"Failed to prepare SQL: " << sqlite3_errmsg16(m_sqliteDb) << L"\n";
        return false;
    }

    // 依次绑定每个参数（使用 UTF-16）
    sqlite3_bind_text16(stmt, 1, record.full_path.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text16(stmt, 2, record.file_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text16(stmt, 3, record.base_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text16(stmt, 4, record.location.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text16(stmt, 5, record.file_type.c_str(), -1, SQLITE_TRANSIENT);

    // 执行语句
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::wcerr << L"Failed to execute insert: " << sqlite3_errmsg16(m_sqliteDb) << L"\n";
        sqlite3_finalize(stmt);
        return false;
    }

    // 清理资源
    sqlite3_finalize(stmt);

    // 可选调试代码
    if (record.file_type == L"RAW") {
        int i = 0; // 保留你原来的调试点
    }

    return true;
}

std::vector<FileRecord> FileDatabase::queryRawOnlyNoJpg()
{
    std::vector<FileRecord> results;
    // SQL: 查询所有 file_type='raw'，且对应 base_name 没有 file_type='jpg' 的记录
    const std::string sql = R"(
        SELECT file_name, base_name, full_path, location, file_type FROM files f1
        WHERE f1.file_type = 'raw'
          AND NOT EXISTS (
              SELECT 1 FROM files f2
              WHERE f2.base_name = f1.base_name
                AND f2.file_type = 'pic'
          );
    )";

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(m_sqliteDb, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare query.\n";
        return results;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        FileRecord r;
        r.file_name = reinterpret_cast<const wchar_t *>(sqlite3_column_text16(stmt, 0));
        r.base_name = reinterpret_cast<const wchar_t *>(sqlite3_column_text16(stmt, 1));
        r.full_path = reinterpret_cast<const wchar_t *>(sqlite3_column_text16(stmt, 2));
        r.location = reinterpret_cast<const wchar_t *>(sqlite3_column_text16(stmt, 3));
        r.file_type = reinterpret_cast<const wchar_t *>(sqlite3_column_text16(stmt, 4));

        results.push_back(r);
    }

    sqlite3_finalize(stmt);
    return results;
}

std::vector<FileRecord> FileDatabase::query(const std::string &whereClause)
{
    std::vector<FileRecord> results;
    std::string sql = "SELECT file_name, base_name, full_path, location, file_type FROM files";
    if (!whereClause.empty()) {
        sql += " WHERE " + whereClause;
    }
    sql += ";";

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(m_sqliteDb, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare query.\n";
        return results;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        FileRecord r;
        r.file_name = reinterpret_cast<const wchar_t *>(sqlite3_column_text16(stmt, 0));
        r.base_name = reinterpret_cast<const wchar_t *>(sqlite3_column_text16(stmt, 1));
        r.full_path = reinterpret_cast<const wchar_t *>(sqlite3_column_text16(stmt, 2));
        r.location = reinterpret_cast<const wchar_t *>(sqlite3_column_text16(stmt, 3));
        r.file_type = reinterpret_cast<const wchar_t *>(sqlite3_column_text16(stmt, 4));

        results.push_back(r);
    }

    sqlite3_finalize(stmt);
    return results;
}

bool FileDatabase::dumpToFile(const std::wstring &filename)
{
    sqlite3 *file_db = nullptr;

    // 打开目标文件数据库（UTF-16 路径支持）
    if (sqlite3_open16(filename.c_str(), &file_db) != SQLITE_OK) {
        std::wcerr << L"Failed to open destination DB: " << filename << L"\n";
        return false;
    }

    // 启动备份从当前数据库 m_sqliteDb 到新打开的 file_db
    sqlite3_backup *backup = sqlite3_backup_init(file_db, "main", m_sqliteDb, "main");
    if (!backup) {
        std::wcerr << L"Failed to init backup: " << sqlite3_errmsg(file_db) << L"\n";
        sqlite3_close(file_db);
        return false;
    }

    // 执行备份（全部一次性复制）
    if (sqlite3_backup_step(backup, -1) != SQLITE_DONE) {
        std::wcerr << L"Backup step failed: " << sqlite3_errmsg(file_db) << L"\n";
        sqlite3_backup_finish(backup);
        sqlite3_close(file_db);
        return false;
    }

    // 清理
    sqlite3_backup_finish(backup);
    sqlite3_close(file_db);
    return true;
}

#include <filesystem>
#include <iostream>
#include <map>
#include <set>
#include <string>
namespace fs = std::filesystem;

#include <fcntl.h>
#include <io.h>

#include "CommonTool.h"
#include "WinTool.h"

#include <sqlite3.h>

class RawRemoveCore
{
public:
    RawRemoveCore() = default;

    std::wstring m_rootDir;
    std::wstring m_rawStoreDir;
    using FilesSetsType = std::map<std::wstring, std::set<std::wstring, CaseInsCmpW>, CaseInsCmpW>;
    FileDatabase m_db;
    enum class FileType
    {
        PIC,
        ARW,
    };

    void ParseFileLocation()
    {
        bool isRawStoreDir = false;
        std::vector<FileRecord> files;
        FindFilesDfs(
            m_rootDir,
            [&](const std::wstring &folderPath, const WIN32_FIND_DATAW &data) {
                FileRecord file;
                fs::path fileFs = (fs::path(folderPath) / data.cFileName).generic_wstring();
                const std::wstring fileFullPath = fs::absolute(fileFs).generic_wstring();

                //const std::wstring
                if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    //std::wcout << L"这是一个目录" << std::endl;
                    //std::wcout << L"这是一个目录" << data.cFileName << std::endl;
                }
                else {
                    fs::path path = data.cFileName;
                    std::wstring fileName = data.cFileName;
                    std::wstring extName = path.extension().wstring();
                    //std::wcout << L"这是一个普通文件" << data.cFileName << std::endl;
                    std::set<std::wstring, CaseInsCmpW> picExts{
                        L".jpg",
                        L".png",
                        L".jpeg",
                    };
                    if (fileName == L"DSC04327.xmp") {
                        int i = 0;
                    }
                    if (picExts.count(extName) > 0) {
                        file.file_type = L"PIC";
                    }
                    else if (CaseInsCmpW::sameIns(extName, L".arw")) {
                        file.file_type = L"RAW";
                    }
                    else {
                        file.file_type = extName;
                    }
                    file.full_path = fileFullPath;
                    file.file_name = fileFs.filename().generic_wstring();
                    file.base_name = fileFs.stem().generic_wstring();
                    file.location = fs::absolute(fileFs.parent_path()).generic_wstring();
                    files.push_back(file);
                }
                return GoOnFind::CONTINUE;
            },
            [&](bool isInto, const std::wstring &folderName) {
                if (fs::weakly_canonical(folderName) == fs::weakly_canonical(m_rawStoreDir)) {
                    isRawStoreDir = isInto;
                }
            });

        m_db.Clear();
        for (const FileRecord &fr : files) {
            m_db.insertOrReplace(fr);
        }
        m_db.dumpToFile(L"./test_db.db");
        auto pics1 = m_db.queryByType(L"PIC");
        auto pics2 = m_db.queryByType(L"RAW");
        FileRecord fileRec;
        fileRec.file_type = L"PIC";
        std::wstring querySql;
        querySql = querySql + FileRecord::Filed2DbKey(&FileRecord::file_type) + LR"( = "PIC")";
        querySql = querySql + LR"( OR )" + FileRecord::Filed2DbKey(&FileRecord::file_type) + LR"( = "RAW")";

        auto pics3 = m_db.query(querySql);
        return;
    }

    void RemoveUnuseRawsTo(const std::wstring &folderPath = L"")
    {
        if (folderPath.empty() == false && fs::exists(folderPath) == false) {
            fs::create_directories(folderPath);
        }

        std::vector<FileRecord> rawsUnuse = m_db.SelectAllBy(LR"(
            FROM files f1
            WHERE f1.file_type = 'raw'
              AND NOT EXISTS (
                  SELECT 1 FROM files f2
                  WHERE f2.base_name = f1.base_name
                    AND f2.file_type = 'PIC'
              );
        )");

        // 删掉/移动
        for (const FileRecord &rawUnuse : rawsUnuse) {
            if (folderPath.empty()) {
                //throw std::exception("禁止删除功能, 请指定raw的移动目标路径替代回收站");
                //fs::remove(rawPath);
                WinTools::moveToRecycleBin(rawUnuse.full_path);
                std::wcout << "remove " << rawUnuse.full_path << std::endl;
            }
            else {
                std::wstring toPath = (fs::path(folderPath) / rawUnuse.file_name).generic_wstring();
                fs::rename(rawUnuse.full_path, toPath);
                std::wcout << "move " << rawUnuse.full_path << " to " << toPath << std::endl;
            }
        }

        this->ParseFileLocation();
        return;
    }

    void MoveAllRawsToStoreDir()
    {
        if (fs::exists(m_rawStoreDir) == false) {
            fs::create_directories(m_rawStoreDir);
        }

        std::vector<FileRecord> raws = m_db.queryByType(L"RAW");
        std::map<fs::path, FileRecord> rawsRec;
        for (size_t i = 0; i < raws.size(); i++) {
            FileRecord &raw = raws[i];
            bool insertRet = rawsRec.insert(std::make_pair(raw.file_name, raw)).second;
            if (insertRet == false) {
                throw std::exception();
            }
            std::wstring newPath = (fs::absolute(m_rawStoreDir) / raw.file_name).generic_wstring();
            fs::rename(raw.full_path, newPath);
        }
        this->ParseFileLocation();
    }

    void MoveRaw2Pic(const std::vector<std::wstring> &picsFileName = {})
    {
        std::vector<FileRecord> raws = m_db.queryByType(L"RAW");
        std::vector<FileRecord> pics = m_db.queryByType(L"PIC");
        // 检查和建立图片索引
        std::map<std::wstring, FileRecord> baseName2Pic;
        for (auto pic : pics) {
            if (baseName2Pic.insert(std::make_pair(pic.base_name, pic)).second == false) {
                // 禁止重复名字的图片干扰
                //throw std::exception();
            }
        }
        // 检查和建立RAW索引
        std::map<std::wstring, FileRecord> baseName2Raw;
        for (auto raw : raws) {
            if (baseName2Raw.insert(std::make_pair(raw.base_name, raw)).second == false) {
                // 禁止重复名字的raw干扰
                throw std::exception();
            }
        }
        // 检查参数索引
        std::set<std::wstring> picsStemName;
        for (std::wstring picFileName : picsFileName) {
            picsStemName.insert(fs::path(picFileName).stem().wstring());
        }

        // 开始移动raw
        for (auto raw : raws) {
            if (picsStemName.empty() == false && picsStemName.count(raw.base_name) == 0) {
                continue;
            }
            if (baseName2Pic.count(raw.base_name) == 0) {
                std::wcout << fmt::format(L"skip for not find the pic [{}]", raw.base_name);
                continue;
            }
            auto pic = baseName2Pic.at(raw.base_name);
            std::wstring newpath = fs::absolute(pic.location) / raw.file_name;
            fs::rename(raw.full_path, newpath);
        }
        this->ParseFileLocation();
    }
};

extern void
moveFilesWithPrefixes(const std::set<std::string> &prefixes, const fs::path &srcDir, const fs::path &dstDir);

extern void testMain();

void main()
{
    return testMain();
    //moveFilesWithPrefixes(
    //    {
    //        "DSC06242", "DSC06247", "DSC06250", "DSC06288", "DSC06300", "DSC06310", "DSC06330", "DSC06363", "DSC06392",
    //        "DSC06393", "DSC06399", "DSC06536", "DSC06547", "DSC06582", "DSC06609", "DSC06623", "DSC06633", "DSC06170",
    //        "DSC06172", "DSC06173", "DSC06176", "DSC06178", "DSC06211", "DSC06232", "DSC06240",
    //    },
    //    R"(D:\_File\a6700\2025-08-02-和倪倪一起委托\倪倪)",
    //    R"(D:\_File\a6700\2025-08-02-和倪倪一起委托\已完成)");
    //return;



    //// 设置控制台为 UTF-8 编码
    //SetConsoleOutputCP(CP_UTF8);
    // 设置控制台为 UTF-16 编码
    int retSetMode = _setmode(_fileno(stdout), _O_U16TEXT);
    std::wcout << L"Hello 控制台" << std::endl;

    RawRemoveCore core;
    std::wstring rootDir = LR"(D:\_File\a6700\100MSDCF)";
    core.m_rootDir = rootDir;
    core.m_rawStoreDir = rootDir + LR"(\ALL_RAW)";

    core.ParseFileLocation();

    core.MoveAllRawsToStoreDir();

    core.RemoveUnuseRawsTo(rootDir + LR"(\unuseRaws)");
    //core.MoveRaw2Pic({L"DSC06024.JPG"});
    //core.MoveRaw2Pic({L"DSC06029.JPG"});
    //core.MoveRaw2Pic({L"DSC05792.JPG"});
    //std::vector<std::wstring> pics = {L"DSC06243.JPG",
    //                                  L"DSC06252.JPG",
    //                                  L"DSC06263.JPG",
    //                                  L"DSC06358.JPG",
    //                                  L"DSC06539.JPG",
    //                                  L"DSC06549.JPG",
    //                                  L"DSC06571.JPG",
    //                                  L"DSC06586.JPG",
    //                                  L"DSC06610.JPG",
    //                                  L"DSC06636.JPG",
    //                                  L"DSC06175.JPG",
    //};
    //core.MoveRaw2Pic(pics);

    return;
}