#include "ArchiveTool.h"

#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <atomic>
#include <mutex>
#include <map>
#include <memory>
#include <thread>
#include <cstring>
namespace fs = std::filesystem;

#include <bit7z/bitfileextractor.hpp>
#include <archive.h>
#include <archive_entry.h>
#include <fmt/format.h>

#include "Global.h"
#include "CommonTool.h"
#include "PwdManager.h"
#include "ArchiveType.h"
#include "ArchiveMsg.h"

// -----------------------------------------------------------------------
// 内部进度数据
// -----------------------------------------------------------------------
struct ProgressItem {
    int         current;
    int         total;
    std::string pwd;
    std::string type;
    int         found;
    int         finished;
};

// 方式二专用：线程安全覆盖写容器
struct LatestHolder {
    std::mutex                    mutex;
    std::shared_ptr<ProgressItem> item;

    void store(std::shared_ptr<ProgressItem> p) {
        std::lock_guard<std::mutex> lk(mutex);
        item = std::move(p);
    }
    std::shared_ptr<ProgressItem> load() {
        std::lock_guard<std::mutex> lk(mutex);
        return item;
    }
};

// -----------------------------------------------------------------------
// 任务管理（std::thread + atomic<bool> 取消标志，不依赖 TBB）
// -----------------------------------------------------------------------
struct TaskData {
    std::shared_ptr<std::atomic<bool>> cancelled;
    std::shared_ptr<LatestHolder>      latest;
};

static std::mutex              g_taskMutex;
static std::map<int, TaskData> g_tasks;
static std::atomic<int>        g_nextTaskId{1};

static int AllocTask(bool withLatest)
{
    int id = g_nextTaskId.fetch_add(1);
    TaskData td;
    td.cancelled = std::make_shared<std::atomic<bool>>(false);
    if (withLatest) td.latest = std::make_shared<LatestHolder>();
    std::lock_guard<std::mutex> lk(g_taskMutex);
    g_tasks[id] = std::move(td);
    return id;
}

static TaskData GetTask(int id)
{
    std::lock_guard<std::mutex> lk(g_taskMutex);
    auto it = g_tasks.find(id);
    return (it != g_tasks.end()) ? it->second : TaskData{};
}

static void RemoveTask(int id)
{
    std::lock_guard<std::mutex> lk(g_taskMutex);
    g_tasks.erase(id);
}

// -----------------------------------------------------------------------
// 内部辅助
// -----------------------------------------------------------------------
static int WriteStrBuf(const std::string &src, char *buf, int bufSize)
{
    int len = static_cast<int>(src.size());
    if (buf && bufSize > 0) {
        int copy = (len < bufSize - 1) ? len : bufSize - 1;
        std::memcpy(buf, src.c_str(), copy);
        buf[copy] = '\0';
    }
    return len;
}

// -----------------------------------------------------------------------
// 实现
// -----------------------------------------------------------------------

ZYB_ARCHIVE_TOOL_API int ArchiveExtraTest(
    const char *file, const char *passwd, const char *type)
{
    std::wstring wfile   = CommonTool::Utf82Wstr(file   ? file   : "");
    std::wstring wpasswd = CommonTool::Utf82Wstr(passwd ? passwd : "");
    std::string  typeU8  = type ? type : "Auto";

    const bit7z::BitInFormat *format = ArchiveType::Ins().GetFormat(typeU8);
    try {
        using namespace bit7z;
        BitFileExtractor extractor{::Get7zLibrary(), *format};
        if (!wpasswd.empty()) extractor.setPassword(CommonTool::Wstr2Utf8(wpasswd));
        extractor.test(CommonTool::Wstr2Utf8(wfile));
    }
    catch (...) { return 0; }
    return 1;
}

ZYB_ARCHIVE_TOOL_API int check_format(const char *filePath, char *buf, int bufSize)
{
    struct archive *a = archive_read_new();
    archive_read_support_format_all(a);
    if (archive_read_open_filename(a, filePath, 500 * 1240) != ARCHIVE_OK) {
        archive_read_free(a); return 0;
    }
    struct archive_entry *entry = nullptr;
    archive_read_next_header(a, &entry);
    const char *fmt = archive_format_name(a);
    std::string result = fmt ? fmt : "";
    archive_read_close(a);
    archive_read_free(a);
    return WriteStrBuf(result, buf, bufSize);
}

ZYB_ARCHIVE_TOOL_API int TryDetermineType(const char *filePath, char *buf, int bufSize)
{
    std::wstring wpath = CommonTool::Utf82Wstr(filePath ? filePath : "");
    ArchiveMsg::Ins().Clear();

    std::vector<std::string> keys = ArchiveType::Ins().GetKeys();
    std::partition(keys.begin(), keys.end(), [](const std::string &s) { return s != "Auto"; });

    std::string result = "Auto";
    for (const std::string &type : keys) {
        const bit7z::BitInFormat *format = ArchiveType::Ins().GetFormat(type);
        bit7z::BitFileExtractor extractor{::Get7zLibrary(), *format};
        try {
            extractor.test(CommonTool::Wstr2Utf8(wpath));
            result = type; break;
        }
        catch (const bit7z::BitException &ex) {
            std::string exMsg = ex.what();
            std::error_code code = ex.code();
            ArchiveMsg::Ins().AddLine(fmt::format("[{}]::[{}]{}", type, code.value(), exMsg));
            int cd = code.value();
            if (exMsg.find("A password is required but none was provided") != std::string::npos
                || cd == 5 || cd == 9) { result = type; break; }
        }
    }
    return WriteStrBuf(result, buf, bufSize);
}

ZYB_ARCHIVE_TOOL_API void GetKeys(EnumKeysCallback callback, void *userData)
{
    if (!callback) return;
    for (const auto &k : ArchiveType::Ins().GetKeys()) callback(k.c_str(), userData);
}

ZYB_ARCHIVE_TOOL_API void UpdateTable(const char *key, const char *type)
{
    if (key && type) ArchiveType::Ins().UpdateTable(std::string(key), std::string(type));
}

ZYB_ARCHIVE_TOOL_API void ArchiveToolMsg(EnumMsgCallback callback, void *userData)
{
    if (!callback) return;
    for (const auto &msg : ArchiveMsg::Ins().MsgLines()) callback(msg.c_str(), userData);
}

ZYB_ARCHIVE_TOOL_API int AddNewPwd(const char *pwd)
{
    if (!pwd) return 0;
    return PwdManager::Ins().AddNewPwd(std::string(pwd)) ? 1 : 0;
}

ZYB_ARCHIVE_TOOL_API void GetAllPwd(EnumPwdCallback callback, void *userData)
{
    if (!callback) return;
    for (const auto &pwd : PwdManager::Ins().GetAllPwd()) callback(pwd.c_str(), userData);
}

ZYB_ARCHIVE_TOOL_API int FindFirstPassword(const char *filePath, char *buf, int bufSize)
{
    char typeBuf[256] = {};
    TryDetermineType(filePath, typeBuf, sizeof(typeBuf));
    for (const auto &pwd : PwdManager::Ins().GetAllPwd()) {
        if (ArchiveExtraTest(filePath, pwd.c_str(), typeBuf))
            return WriteStrBuf(pwd, buf, bufSize);
    }
    return 0;
}

// -----------------------------------------------------------------------
// 方式一：完整回调（std::thread）
// -----------------------------------------------------------------------
ZYB_ARCHIVE_TOOL_API int FindPasswordAsync(
    const char *filePath, FindPasswordCallback callback, void *userData, int findAll)
{
    if (!filePath || !callback) return 0;

    int taskId = AllocTask(false);
    TaskData td = GetTask(taskId);
    std::string filePathStr(filePath);

    std::thread([filePathStr, callback, userData, findAll, taskId, td]() {
        char typeBuf[256] = {};
        TryDetermineType(filePathStr.c_str(), typeBuf, sizeof(typeBuf));

        std::vector<std::string> pwds = PwdManager::Ins().GetAllPwd();
        int total = static_cast<int>(pwds.size());

        for (int i = 0; i < total; ++i) {
            if (td.cancelled->load()) break;

            int found = ArchiveExtraTest(filePathStr.c_str(), pwds[i].c_str(), typeBuf);

            FindPasswordProgress progress{};
            progress.current  = i + 1;
            progress.total    = total;
            progress.pwd      = pwds[i].c_str();
            progress.type     = typeBuf;
            progress.found    = found;
            progress.finished = 0;

            if (!callback(&progress, userData)) break;
            if (!findAll && found) break;
        }

        FindPasswordProgress done{};
        done.current = done.total = total;
        done.pwd = ""; done.type = typeBuf;
        done.found = 0; done.finished = 1;
        callback(&done, userData);

        RemoveTask(taskId);
    }).detach();

    return taskId;
}

// -----------------------------------------------------------------------
// 方式二：信号 + 覆盖写（std::thread）
// -----------------------------------------------------------------------
ZYB_ARCHIVE_TOOL_API int FindPasswordAsyncQueue(
    const char *filePath, FindPasswordSignalCallback signalCallback, void *userData, int findAll)
{
    if (!filePath || !signalCallback) return 0;

    int taskId = AllocTask(true);
    TaskData td = GetTask(taskId);
    std::string filePathStr(filePath);

    std::thread([filePathStr, signalCallback, userData, findAll, taskId, td]() {
        char typeBuf[256] = {};
        TryDetermineType(filePathStr.c_str(), typeBuf, sizeof(typeBuf));

        std::vector<std::string> pwds = PwdManager::Ins().GetAllPwd();
        int total = static_cast<int>(pwds.size());

        for (int i = 0; i < total; ++i) {
            if (td.cancelled->load()) break;

            int found = ArchiveExtraTest(filePathStr.c_str(), pwds[i].c_str(), typeBuf);

            auto item = std::make_shared<ProgressItem>();
            item->current  = i + 1;
            item->total    = total;
            item->pwd      = pwds[i];
            item->type     = typeBuf;
            item->found    = found;
            item->finished = 0;
            td.latest->store(item);

            signalCallback(taskId, userData);
            if (!findAll && found) break;
        }

        auto done = std::make_shared<ProgressItem>();
        done->current = done->total = total;
        done->found = 0; done->finished = 1;
        td.latest->store(done);
        signalCallback(taskId, userData);

        RemoveTask(taskId);
    }).detach();

    return taskId;
}

ZYB_ARCHIVE_TOOL_API int GetLatestFindPasswordProgress(
    int taskId, struct FindPasswordProgressOut *out)
{
    if (!out) return 0;
    TaskData td = GetTask(taskId);
    if (!td.latest) return 0;

    auto item = td.latest->load();
    if (!item) return 0;

    out->current  = item->current;
    out->total    = item->total;
    out->found    = item->found;
    out->finished = item->finished;
    WriteStrBuf(item->pwd,  out->pwd,  sizeof(out->pwd));
    WriteStrBuf(item->type, out->type, sizeof(out->type));
    return 1;
}

ZYB_ARCHIVE_TOOL_API void CancelFindPassword(int taskId)
{
    TaskData td = GetTask(taskId);
    if (td.cancelled) td.cancelled->store(true);
}
