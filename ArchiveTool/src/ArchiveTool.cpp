#include "ArchiveTool.h"

#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <atomic>
#include <mutex>
#include <map>
#include <memory>
#include <cstring>
namespace fs = std::filesystem;

#include <tbb/task_group.h>
#include <tbb/concurrent_queue.h>

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
// 内部进度数据（带字符串副本，用于队列存储）
// -----------------------------------------------------------------------
struct ProgressItem {
    int         current;
    int         total;
    std::string pwd;
    std::string type;
    int         found;
    int         finished;
};

// -----------------------------------------------------------------------
// 任务数据（方式一 和 方式二 共用 task_group + 取消标志）
// -----------------------------------------------------------------------
struct TaskData {
    std::shared_ptr<tbb::task_group>          tg;
    std::shared_ptr<std::atomic<bool>>        cancelled;
    // 方式二专用：线程安全进度队列
    std::shared_ptr<tbb::concurrent_queue<ProgressItem>> queue;
};

static std::mutex              g_taskMutex;
static std::map<int, TaskData> g_tasks;
static std::atomic<int>        g_nextTaskId{1};

static int AllocTask(bool withQueue)
{
    int id = g_nextTaskId.fetch_add(1);
    TaskData td;
    td.tg        = std::make_shared<tbb::task_group>();
    td.cancelled = std::make_shared<std::atomic<bool>>(false);
    if (withQueue) {
        td.queue = std::make_shared<tbb::concurrent_queue<ProgressItem>>();
    }
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
        if (!wpasswd.empty()) {
            extractor.setPassword(CommonTool::Wstr2Utf8(wpasswd));
        }
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
        archive_read_free(a);
        return 0;
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
// 方式一：完整回调（TBB task_group）
// -----------------------------------------------------------------------
ZYB_ARCHIVE_TOOL_API int FindPasswordAsync(
    const char *filePath, FindPasswordCallback callback, void *userData, int findAll)
{
    if (!filePath || !callback) return 0;

    int taskId = AllocTask(false);
    TaskData td = GetTask(taskId);
    std::string filePathStr(filePath);

    td.tg->run([filePathStr, callback, userData, findAll, taskId, td]() {
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

        // 完成通知
        FindPasswordProgress done{};
        done.current = done.total = total;
        done.pwd = ""; done.type = typeBuf;
        done.found = 0; done.finished = 1;
        callback(&done, userData);

        RemoveTask(taskId);
    });

    return taskId;
}

// -----------------------------------------------------------------------
// 方式二：信号 + tbb::concurrent_queue
// -----------------------------------------------------------------------
ZYB_ARCHIVE_TOOL_API int FindPasswordAsyncQueue(
    const char *filePath, FindPasswordSignalCallback signalCallback, void *userData, int findAll)
{
    if (!filePath || !signalCallback) return 0;

    int taskId = AllocTask(true);
    TaskData td = GetTask(taskId);
    std::string filePathStr(filePath);

    td.tg->run([filePathStr, signalCallback, userData, findAll, taskId, td]() {
        char typeBuf[256] = {};
        TryDetermineType(filePathStr.c_str(), typeBuf, sizeof(typeBuf));

        std::vector<std::string> pwds = PwdManager::Ins().GetAllPwd();
        int total = static_cast<int>(pwds.size());

        for (int i = 0; i < total; ++i) {
            if (td.cancelled->load()) break;

            int found = ArchiveExtraTest(filePathStr.c_str(), pwds[i].c_str(), typeBuf);

            // 将进度存入线程安全队列
            ProgressItem item;
            item.current  = i + 1;
            item.total    = total;
            item.pwd      = pwds[i];
            item.type     = typeBuf;
            item.found    = found;
            item.finished = 0;
            td.queue->push(item);

            // 只发送信号，不传数据
            signalCallback(taskId, userData);

            if (!findAll && found) break;
        }

        // 完成通知
        ProgressItem done{};
        done.current = done.total = total;
        done.found = 0; done.finished = 1;
        td.queue->push(done);
        signalCallback(taskId, userData);

        RemoveTask(taskId);
    });

    return taskId;
}

ZYB_ARCHIVE_TOOL_API int PopFindPasswordProgress(
    int taskId, struct FindPasswordProgressOut *out)
{
    if (!out) return 0;
    TaskData td = GetTask(taskId);
    if (!td.queue) return 0;

    ProgressItem item;
    if (!td.queue->try_pop(item)) return 0;

    out->current  = item.current;
    out->total    = item.total;
    out->found    = item.found;
    out->finished = item.finished;
    WriteStrBuf(item.pwd,  out->pwd,  sizeof(out->pwd));
    WriteStrBuf(item.type, out->type, sizeof(out->type));
    return 1;
}

ZYB_ARCHIVE_TOOL_API void CancelFindPassword(int taskId)
{
    TaskData td = GetTask(taskId);
    if (td.cancelled) {
        td.cancelled->store(true);
        // 请求 TBB 取消任务组（对 task_group 内的子任务生效）
        td.tg->cancel();
    }
}
