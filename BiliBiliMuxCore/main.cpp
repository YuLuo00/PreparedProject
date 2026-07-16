#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>

extern "C"
{
    #include "libavcodec/avcodec.h"
    #include "libavformat/avformat.h"
    #include "libavformat/avio.h"
    #include "libavutil/avutil.h"
}

extern "C"
{
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libavutil/avutil.h>
    #include <libavutil/channel_layout.h>
    #include <libavutil/opt.h>
    #include <libavutil/samplefmt.h>
    #include <libswresample/swresample.h>
}

#include <atomic>
#include <filesystem>
#include <functional>  // std::hash
#include <map>
#include <mutex>
#include <queue>
#include <set>
#include <shared_mutex>
#include <string>
#include <fstream>
#include <iosfwd>
#include <sstream>
#include <iostream>
#include <functional>  // std::reference_wrapper
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>
#include <memory>
#include <system_error>
using namespace std;

#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/mem.h>

#include <tbb/concurrent_queue.h>
#include <tbb/flow_graph.h>
using namespace tbb::flow;

#include "attach.h"
#include "AvApiWrapper.h"
#include "BiliApiCache.h"
#include "BiliApiClient.h"
#include "BiliCache.h"
#include "common.h"
#include "ffmpegMsg.h"
#include "Logger.h"
#include "MediaMux.h"
#include "ReadNode.h"
#include "tools.h"
#include "MatchCollector.h"

#include <algorithm>
#include <vector>


std::string ToLowerAscii(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return text;
}

std::string TrimQueryAndFragment(const std::string &url)
{
    size_t end = url.find_first_of("?#");
    if (end == std::string::npos) {
        end = url.size();
    }
    return url.substr(0, end);
}

bool IsSafeExtension(const std::string &ext)
{
    static const std::vector<std::string> safeExts = {".jpg", ".jpeg", ".png", ".webp", ".gif", ".bmp", ".tiff", ".tif"};
    return std::find(safeExts.begin(), safeExts.end(), ext) != safeExts.end();
}

std::string GuessExtensionFromUrl(const std::string &url)
{
    const std::string cleanUrl = TrimQueryAndFragment(url);
    const size_t slashPos = cleanUrl.find_last_of('/');
    const size_t dotPos = cleanUrl.find_last_of('.');
    if (dotPos == std::string::npos) {
        return {};
    }
    if (slashPos != std::string::npos && dotPos < slashPos) {
        return {};
    }

    std::string ext = ToLowerAscii(cleanUrl.substr(dotPos));
    if (!IsSafeExtension(ext)) {
        return {};
    }

    return ext;
}

std::string GuessExtensionFromContentType(const std::string &contentType)
{
    const std::string lowerType = ToLowerAscii(contentType);
    if (lowerType.find("image/jpeg") != std::string::npos || lowerType.find("image/jpg") != std::string::npos) {
        return ".jpg";
    }
    if (lowerType.find("image/png") != std::string::npos) {
        return ".png";
    }
    if (lowerType.find("image/webp") != std::string::npos) {
        return ".webp";
    }
    if (lowerType.find("image/gif") != std::string::npos) {
        return ".gif";
    }
    if (lowerType.find("image/bmp") != std::string::npos) {
        return ".bmp";
    }
    if (lowerType.find("image/tiff") != std::string::npos) {
        return ".tiff";
    }
    return {};
}

std::string BuildCoverBaseName(const MediaInfo &mediaInfo)
{
    if (!mediaInfo.download_title.empty()) {
        return Tools::wstring_to_utf8(mediaInfo.download_title);
    }
    if (!mediaInfo.title.empty()) {
        return Tools::wstring_to_utf8(mediaInfo.title);
    }
    if (!mediaInfo.bvid.empty()) {
        return Tools::wstring_to_utf8(mediaInfo.bvid);
    }
    if (mediaInfo.avid > 0) {
        return "av" + std::to_string(mediaInfo.avid);
    }
    return {};
}

std::string GenerateUniqueCoverName(const std::string& coverUrl)
{
    // 从URL中提取文件名部分
    size_t lastSlash = coverUrl.find_last_of('/');
    if (lastSlash != std::string::npos) {
        std::string filename = coverUrl.substr(lastSlash + 1);
        // 移除查询参数
        size_t questionMark = filename.find('?');
        if (questionMark != std::string::npos) {
            filename = filename.substr(0, questionMark);
        }
        return filename;
    }
    // 如果无法从URL提取，使用哈希值作为文件名
    std::hash<std::string> hasher;
    return "cover_" + std::to_string(hasher(coverUrl)) + ".jpg";
}

std::filesystem::path BuildCoverFilePathImpl(const MediaInfo &mediaInfo,
                                             const std::filesystem::path &outputDir,
                                             const std::string &preferredExt)
{
    std::string baseNameStr = BuildCoverBaseName(mediaInfo);
    std::wstring baseName = Tools::sanitize_windows_filename(Tools::utf8_to_wstring(baseNameStr));
    if (baseName.empty()) {
        baseName = L"cover";
    }

    std::string ext = preferredExt;
    if (ext.empty()) {
        try {
            ext = GuessExtensionFromUrl(Tools::wstring_to_utf8(mediaInfo.cover));
        }
        catch (...) {
            ext.clear();
        }
    }
    if (ext.empty()) {
        ext = ".jpg";
    }

    const std::string fileNameUtf8 = Tools::wstring_to_utf8(baseName) + "_cover" + ext;
    return outputDir / std::filesystem::path(fileNameUtf8);
}

fs::path BuildUniqueOutputPath(const fs::path &outputDir, const std::wstring &baseName, const std::wstring &extension)
{
    fs::path candidate = outputDir / (baseName + extension);
    if (!fs::exists(candidate)) {
        return candidate;
    }

    for (int i = 2; i < 10000; ++i) {
        candidate = outputDir / (baseName + L"_" + std::to_wstring(i) + extension);
        if (!fs::exists(candidate)) {
            return candidate;
        }
    }

    return outputDir / (baseName + L"_" + std::to_wstring(
        std::chrono::system_clock::now().time_since_epoch().count()) + extension);
}

bool WriteUtf8TextFile(const fs::path &path, const std::string &content, std::string *error = nullptr)
{
    try {
        if (!path.parent_path().empty()) {
            fs::create_directories(path.parent_path());
        }

        std::ofstream ofs(path, std::ios::out | std::ios::binary | std::ios::trunc);
        if (!ofs) {
            if (error) {
                *error = "Failed to open output file: " + Tools::wstring_to_utf8(path.wstring());
            }
            return false;
        }

        ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
        if (!ofs) {
            if (error) {
                *error = "Failed to write output file: " + Tools::wstring_to_utf8(path.wstring());
            }
            return false;
        }
        return true;
    }
    catch (const std::exception &e) {
        if (error) {
            *error = e.what();
        }
        return false;
    }
}

int ExtractDocumentsFromMp4(const fs::path &mp4Path, const fs::path &outputRoot)
{
    fs::path outputDir = outputRoot.empty() ? mp4Path.parent_path() : outputRoot;
    if (outputDir.empty()) {
        outputDir = fs::current_path();
    }
    if (outputDir.empty()) {
        LOG_ERROR(LogGroup::IO, "Extract output directory is empty.");
        return 2;
    }

    AVFormatContext *fmt = nullptr;
    const std::string inputPath = Tools::wstring_to_utf8(mp4Path.wstring());
    int ret = avformat_open_input(&fmt, inputPath.c_str(), nullptr, nullptr);
    if (ret < 0) {
        char errMsg[AV_ERROR_MAX_STRING_SIZE] = {'\0'};
        av_strerror(ret, errMsg, sizeof(errMsg));
        LOG_ERROR(LogGroup::IO, "Open mp4 failed: {}, {}", LOGUTF8(mp4Path.wstring()), errMsg);
        return ret;
    }

    ret = avformat_find_stream_info(fmt, nullptr);
    if (ret < 0) {
        char errMsg[AV_ERROR_MAX_STRING_SIZE] = {'\0'};
        av_strerror(ret, errMsg, sizeof(errMsg));
        LOG_ERROR(LogGroup::IO, "Read mp4 stream info failed: {}, {}", LOGUTF8(mp4Path.wstring()), errMsg);
        avformat_close_input(&fmt);
        return ret;
    }

    const AVDictionaryEntry *commentEntry = av_dict_get(fmt->metadata, "comment", nullptr, 0);
    if (!commentEntry || !commentEntry->value || commentEntry->value[0] == '\0') {
        LOG_ERROR(LogGroup::IO, "No embedded document metadata found in mp4 comment tag: {}", LOGUTF8(mp4Path.wstring()));
        avformat_close_input(&fmt);
        return 3;
    }

    json documentJson;
    try {
        documentJson = json::parse(commentEntry->value);
    }
    catch (const std::exception &e) {
        LOG_ERROR(LogGroup::IO, "Embedded document metadata is not valid JSON: {}", e.what());
        avformat_close_input(&fmt);
        return 4;
    }
    avformat_close_input(&fmt);

    std::string error;
    fs::create_directories(outputDir);

    const fs::path combinedPath = outputDir / "bilibili_metadata.json";
    if (!WriteUtf8TextFile(combinedPath, documentJson.dump(4), &error)) {
        LOG_ERROR(LogGroup::IO, "Write metadata failed: {}", error);
        return 5;
    }
    LOG_INFO(LogGroup::IO, "Extracted metadata: {}", LOGUTF8(combinedPath.wstring()));

    bool wroteDocument = false;
    if (documentJson.contains("entry")) {
        const fs::path entryPath = outputDir / "entry.json";
        if (!WriteUtf8TextFile(entryPath, documentJson["entry"].dump(4), &error)) {
            LOG_ERROR(LogGroup::IO, "Write entry.json failed: {}", error);
            return 5;
        }
        wroteDocument = true;
        LOG_INFO(LogGroup::IO, "Extracted entry.json: {}", LOGUTF8(entryPath.wstring()));
    }

    if (documentJson.contains("index")) {
        const fs::path indexPath = outputDir / "index.json";
        if (!WriteUtf8TextFile(indexPath, documentJson["index"].dump(4), &error)) {
            LOG_ERROR(LogGroup::IO, "Write index.json failed: {}", error);
            return 5;
        }
        wroteDocument = true;
        LOG_INFO(LogGroup::IO, "Extracted index.json: {}", LOGUTF8(indexPath.wstring()));
    }

    if (!wroteDocument) {
        const fs::path mediaInfoPath = outputDir / "media_info.json";
        if (!WriteUtf8TextFile(mediaInfoPath, documentJson.dump(4), &error)) {
            LOG_ERROR(LogGroup::IO, "Write media_info.json failed: {}", error);
            return 5;
        }
        LOG_WARN(LogGroup::IO, "No entry/index document keys found; wrote metadata JSON instead: {}", LOGUTF8(mediaInfoPath.wstring()));
    }

    return 0;
}


#include <mutex>
#include <cstdarg>
#include <cstdio>
#include <cstring>



std::mutex ffmpeg_log_mutex;

static spdlog::level::level_enum FFmpegLevelToSpd(int level)
{
    if (level <= AV_LOG_PANIC)
        return spdlog::level::critical;
    if (level <= AV_LOG_FATAL)
        return spdlog::level::critical;
    if (level <= AV_LOG_ERROR)
        return spdlog::level::err;
    if (level <= AV_LOG_WARNING)
        return spdlog::level::warn;
    if (level <= AV_LOG_INFO)
        return spdlog::level::info;
    if (level <= AV_LOG_VERBOSE)
        return spdlog::level::debug;
    return spdlog::level::trace;
}

void my_ffmpeg_log_callback(void *ptr, int level, const char *fmt, va_list vl)
{
    std::lock_guard<std::mutex> lock(ffmpeg_log_mutex);

    // ❗注意：FFmpeg 是 level 越小越严重
    // 如果你只想要 warning 及以上：<std::string>
    if (level > AV_LOG_WARNING) {
        return;
    }

    char buf[2048];
    vsnprintf(buf, sizeof(buf), fmt, vl);

    // 去掉末尾换行
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') {
        buf[len - 1] = '\0';
    }

    auto logger = Logger::Get(LogGroup::FFMPEG);
    auto spd_level = FFmpegLevelToSpd(level);

    // 用 spdlog 的 level 控制输出
    if (logger->should_log(spd_level)) {
        logger->log(spd_level, "{}", buf);
    }

    // ❗是否保留 FFmpeg 默认输出（二选一）
    // 一般建议关掉，否则会重复打印
    // av_log_default_callback(ptr, level, fmt, vl);
}
void GlobalInit(const Logger::Options &loggerOptions)
{
    Logger::Init(loggerOptions);
    Logger::SetLevel(LogGroup::MUX, spdlog::level::info);
    Logger::SetLevel(LogGroup::DECODE, spdlog::level::err);
    Logger::SetLevel(LogGroup::FFMPEG, spdlog::level::debug);
    Logger::SetLevel(LogGroup::IO, spdlog::level::debug);
    av_log_set_callback(my_ffmpeg_log_callback);
}

int mainPipeline(const MediaSubdir &sub, const fs::path &outputRoot = fs::path())
{
    const Match *match = sub.match;
    if (match == nullptr) {
        LOG_ERROR(LogGroup::MUX, "sub.match 为空，无法提取 mediainfo");
        return -1;
    }
    if (sub.audio.empty() || sub.video.empty()) {
        LOG_ERROR(LogGroup::MUX, "MediaSubdir 缺少 audio/video，无法混流");
        return -2;
    }

    MediaInfo mediaInfo;
    if (!loadSubdir2MediaInfo(sub, mediaInfo)) {
        return -3;
    }

    std::wstring title = mediaInfo.title;
    if (title.empty()) {
        title = match->dir.filename().wstring();
    }
    if (title.empty()) {
        title = L"output";
    }

    // 输出文件名优先使用 download_subtitle + page（如果存在），否则回退到 title 或目录名
    std::wstring outBase;
    if (!mediaInfo.download_subtitle.empty()) {
        outBase = mediaInfo.download_subtitle;
    }
    else if (!mediaInfo.title.empty()) {
        outBase = mediaInfo.title;
    }
    else {
        outBase = match->dir.filename().wstring();
    }
    if (outBase.empty()) {
        outBase = L"output";
    }
    if (mediaInfo.page > 0) {
        outBase += L"_" + std::to_wstring(mediaInfo.page);
    }

    const std::wstring safeTitle = Tools::sanitize_windows_filename(outBase);
    const fs::path outputDir = outputRoot.empty() ? (sub.dir.empty() ? fs::current_path() : sub.dir) : outputRoot;
    fs::create_directories(outputDir);
    const fs::path outputPath = BuildUniqueOutputPath(outputDir, safeTitle, L".mp4");
    const std::set<std::string> files = {
        Tools::wstring_to_utf8(sub.audio.wstring()),
        Tools::wstring_to_utf8(sub.video.wstring()),
    };

    LOG_INFO(LogGroup::MUX, "开始混流: {}", LOGUTF8(title));
    LOG_INFO(LogGroup::MUX, "输出文件: {}", LOGUTF8(outputPath.wstring()));

    MediaMux mux;
    std::wstring outputPathU8 = outputPath.wstring();
    std::string opU8 = Tools::wstring_to_utf8(outputPathU8);
    int ret = mux.Open(files, opU8);
    if (ret != 0) {
        return ret;
    }

    ret = write_mediainfo_to_avformat(mux.m_outCtx, sub);
    if (ret != 0) {
        LOG_ERROR(LogGroup::MUX, "写入 metadata 失败: {}", ret);
        mux.Close();
        return ret;
    }

    // 在混流之前下载并嵌入封面
    if (!mediaInfo.cover.empty()) {
        LOG_INFO(LogGroup::MUX, "开始下载封面...");
        BiliApiCache apiCache;
        // 设置缓存根目录为工作目录下的 cache/bilibili_api
        BiliApiCache::Options cacheOptions;
        cacheOptions.cache_root = fs::current_path() / "cache" / "bilibili_api";
        apiCache.SetOptions(cacheOptions);

        std::string coverUrlUtf8 = Tools::wstring_to_utf8(mediaInfo.cover);
        fs::path coverPath;
        std::string errMsg;

        if (apiCache.DownloadCoverToCache(coverUrlUtf8, coverPath, &errMsg, 1000)) {
            // 将封面嵌入到MP4文件中
            int embedRet = mux.EmbedCover(coverPath);
            if (embedRet == 0) {
                LOG_INFO(LogGroup::MUX, "封面已准备嵌入到MP4文件");
            } else {
                LOG_ERROR(LogGroup::MUX, "准备嵌入封面失败: {}", embedRet);
            }
        } else {
            LOG_ERROR(LogGroup::MUX, "下载封面失败: {}", errMsg);
        }
    } else {
        LOG_WARN(LogGroup::MUX, "没有封面信息，跳过封面嵌入");
    }

    ret = mux.mux();
    if (ret != 0) {
        LOG_ERROR(LogGroup::MUX, "混流失败: {}", ret);
        return ret;
    }

    LOG_INFO(LogGroup::MUX, "混流完成: {}", LOGUTF8(outputPath.filename().wstring()));
    return 0;
}

int main_test()
{
    LOG_INFO(LogGroup::DEFAULT, "--------------------------------------main_test begin-----------------------------------");

    fs::path root = R"(C:\Users\Administrator\Desktop\bili_zip_1)";
    auto matches = BiliCache::CollectBiliFoldersStructured(root);
    std::set<int> ownerIdSet;
    std::set<std::wstring> coverSet;

    for (const auto &m : matches) {
        std::string title = BiliCache::GetTitle(m);
        LOG_INFO(LogGroup::IO, "{}", LOGUTF8(title));

        for (size_t i = 0; i < m.media_subdirs.size(); ++i) {
            const MediaSubdir &sub = m.media_subdirs[i];

            MediaInfo mediaInfo;
            if (!loadSubdir2MediaInfo(sub, mediaInfo)) {
                LOG_WARN(LogGroup::IO, "读取 MediaInfo 失败，跳过当前条目");
                continue;
            }

            if (mediaInfo.owner_id > 0) {
                ownerIdSet.insert(mediaInfo.owner_id);
            }
            if (!mediaInfo.cover.empty()) {
                coverSet.insert(mediaInfo.cover);
            }
        }
    }

    LOG_INFO(LogGroup::IO, "Found {} matching folders.", matches.size());
    LOG_INFO(LogGroup::IO, "Collected {} unique owner IDs.", ownerIdSet.size());
    LOG_INFO(LogGroup::IO, "Collected {} unique covers.", coverSet.size());

    LOG_INFO(LogGroup::IO, "Owner IDs:");
    for (const auto &ownerId : ownerIdSet) {
        LOG_INFO(LogGroup::IO, "owner_id={}", ownerId);
    }

    LOG_INFO(LogGroup::IO, "Covers:");
    for (const auto &cover : coverSet) {
        LOG_INFO(LogGroup::IO, "cover={}", LOGUTF8(cover));
    }

    // 测试前三个 owner_id 和 cover
    LOG_INFO(LogGroup::IO, "--------------------------------------Testing API calls-----------------------------------");

    BiliApiCache apiCache;
    // 设置缓存根目录为工作目录下的 cache/bilibili_api
    BiliApiCache::Options cacheOptions;
    cacheOptions.cache_root = fs::current_path() / "cache" / "bilibili_api";
    apiCache.SetOptions(cacheOptions);

    // 获取前三个 owner_id 进行测试 - 暂时注释掉
    /*
    auto ownerIdIt = ownerIdSet.begin();
    for (int i = 0; i < 3 && ownerIdIt != ownerIdSet.end(); ++i, ++ownerIdIt) {
        int ownerId = *ownerIdIt;
        LOG_INFO(LogGroup::IO, "Testing owner_id: {}", ownerId);

        BiliApiClient::AuthorInfo authorInfo;
        std::string errMsg;
        if (apiClient.GetAuthorInfo(ownerId, authorInfo, &errMsg)) {
            LOG_INFO(LogGroup::IO, "Successfully got author info for owner_id {}: name={}", ownerId, LOGUTF8(authorInfo.name));
        } else {
            LOG_ERROR(LogGroup::IO, "Failed to get author info for owner_id {}: {}", ownerId, errMsg);
        }
        Sleep(5000); // 增加到5秒延迟
    }
    */


    auto coverIt = coverSet.begin();
    for (int i = 0;  coverIt != coverSet.end(); ++i, ++coverIt) {
        const std::wstring& coverUrl = *coverIt;
        LOG_INFO(LogGroup::IO, "Testing cover: {}", LOGUTF8(coverUrl));

        std::string coverUrlUtf8 = Tools::wstring_to_utf8(coverUrl);
        fs::path cachedPath;
        std::string errMsg;

        if (apiCache.DownloadCoverToCache(coverUrlUtf8, cachedPath, &errMsg, 500)) {
            LOG_INFO(LogGroup::IO, "Cover ready at: {}", cachedPath.string());
        } else {
            LOG_ERROR(LogGroup::IO, "Failed to get cover {}: {}", coverUrlUtf8, errMsg);
        }
    }

    LOG_INFO(LogGroup::IO, "--------------------------------------main_test end-----------------------------------");

    return 0;
}

extern int add_cover_to_video(const char* output_filename, const char* input_filename, const char* image_filename);

struct AppOptions
{
    fs::path root;
    fs::path output;
    fs::path finished;
    fs::path extractMp4;
    fs::path extractOutput;
    bool showHelp = false;
    bool runTest = false;
    Logger::Options logger;
};

std::vector<std::wstring> GetCommandLineArgs()
{
    std::vector<std::wstring> args;

    int argc = 0;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv == nullptr) {
        return args;
    }

    args.reserve(static_cast<size_t>(argc));
    for (int i = 0; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }
    LocalFree(argv);
    return args;
}

std::string ArgToUtf8(const std::wstring &arg)
{
    return Tools::wstring_to_utf8(arg);
}

void PrintUsage()
{
    std::cout
        << "Usage:\n"
        << "  BiliBiliMuxCore.exe --path <bilibili-cache-root> [options]\n\n"
        << "  BiliBiliMuxCore.exe --extract <mp4> [--extract-output <dir>] [log options]\n\n"
        << "Options:\n"
        << "  --path <dir>             Root directory to scan. Required unless --test is used.\n"
        << "  --output <dir>           Directory for generated mp4 files. Default: each source media folder.\n"
        << "  --finished <dir>         Move processed folders to this directory. Disabled when omitted.\n"
        << "  --extract <mp4>          Extract embedded document metadata from an mp4.\n"
        << "  --extract-output <dir>   Directory for extracted JSON files. Default: mp4 directory.\n"
        << "  --log-file <file>        Log file path. Default: logs/app.log.\n"
        << "  --console-level <level>  Console threshold: trace/debug/info/warn/err/critical/off.\n"
        << "  --file-level <level>     File threshold: trace/debug/info/warn/err/critical/off.\n"
        << "  --no-console-log         Disable console log output.\n"
        << "  --no-file-log            Disable file log output.\n"
        << "  --test                   Run the existing diagnostic test entry.\n"
        << "  --help                   Show this help.\n";
}

bool ParseOptions(const std::vector<std::wstring> &args, AppOptions &options, std::string &error)
{
    for (size_t i = 1; i < args.size(); ++i) {
        const std::wstring &arg = args[i];

        auto requireValue = [&](const char *name) -> const std::wstring * {
            if (i + 1 >= args.size()) {
                error = std::string("Missing value for ") + name;
                return nullptr;
            }
            return &args[++i];
        };

        if (arg == L"--help" || arg == L"-h") {
            options.showHelp = true;
        }
        else if (arg == L"--test") {
            options.runTest = true;
        }
        else if (arg == L"--path") {
            const std::wstring *value = requireValue("--path");
            if (value == nullptr) {
                return false;
            }
            options.root = fs::path(*value);
        }
        else if (arg == L"--output") {
            const std::wstring *value = requireValue("--output");
            if (value == nullptr) {
                return false;
            }
            options.output = fs::path(*value);
        }
        else if (arg == L"--finished") {
            const std::wstring *value = requireValue("--finished");
            if (value == nullptr) {
                return false;
            }
            options.finished = fs::path(*value);
        }
        else if (arg == L"--extract") {
            const std::wstring *value = requireValue("--extract");
            if (value == nullptr) {
                return false;
            }
            options.extractMp4 = fs::path(*value);
        }
        else if (arg == L"--extract-output") {
            const std::wstring *value = requireValue("--extract-output");
            if (value == nullptr) {
                return false;
            }
            options.extractOutput = fs::path(*value);
        }
        else if (arg == L"--log-file") {
            const std::wstring *value = requireValue("--log-file");
            if (value == nullptr) {
                return false;
            }
            options.logger.file_path = Tools::wstring_to_utf8(*value);
        }
        else if (arg == L"--console-level") {
            const std::wstring *value = requireValue("--console-level");
            if (value == nullptr) {
                return false;
            }
            options.logger.console_level = Logger::ParseLevel(ArgToUtf8(*value), options.logger.console_level);
        }
        else if (arg == L"--file-level") {
            const std::wstring *value = requireValue("--file-level");
            if (value == nullptr) {
                return false;
            }
            options.logger.file_level = Logger::ParseLevel(ArgToUtf8(*value), options.logger.file_level);
        }
        else if (arg == L"--no-console-log") {
            options.logger.enable_console = false;
        }
        else if (arg == L"--no-file-log") {
            options.logger.enable_file = false;
        }
        else {
            error = "Unknown argument: " + ArgToUtf8(arg);
            return false;
        }
    }

    if (!options.showHelp && !options.runTest && options.extractMp4.empty() && options.root.empty()) {
        error = "Missing required argument: --path <dir>";
        return false;
    }

    if (!options.extractMp4.empty() && !options.root.empty()) {
        error = "--extract cannot be used together with --path";
        return false;
    }

    return true;
}

int RunMux(const AppOptions &options)
{
    LOG_INFO(LogGroup::DEFAULT, "--------------------------------------run begin-----------------------------------");
    LOG_INFO(LogGroup::IO, "Scan path: {}", LOGUTF8(options.root.wstring()));
    const std::string outputLogPath = options.output.empty() ? std::string("<source media folder>") : LOGUTF8(options.output.wstring());
    LOG_INFO(LogGroup::IO, "Output path: {}", outputLogPath);
    const std::string finishedLogPath = options.finished.empty() ? std::string("<disabled>") : LOGUTF8(options.finished.wstring());
    LOG_INFO(LogGroup::IO, "Finished path: {}", finishedLogPath);

    if (!fs::exists(options.root) || !fs::is_directory(options.root)) {
        LOG_ERROR(LogGroup::IO, "Path does not exist or is not a directory: {}", LOGUTF8(options.root.wstring()));
        return 2;
    }

    std::unique_ptr<MatchCollector> col;
    if (!options.finished.empty()) {
        fs::create_directories(options.finished);
        col = std::make_unique<MatchCollector>(options.finished);
    }
    if (!options.output.empty()) {
        fs::create_directories(options.output);
    }

    auto matches = BiliCache::CollectBiliFoldersStructured(options.root);
    int lastError = 0;

    for (const auto &m : matches) {
        std::string title = BiliCache::GetTitle(m);
        LOG_INFO(LogGroup::IO, "{}", LOGUTF8(title));

        for (size_t i = 0; i < m.media_subdirs.size(); ++i) {
            MediaSubdir sub = m.media_subdirs[i];
            const int ret = mainPipeline(sub, options.output);
            if (ret != 0) {
                lastError = ret;
                LOG_ERROR(LogGroup::MUX, "Pipeline failed: ret={}, subdir={}", ret, LOGUTF8(sub.dir.wstring()));
            }
            if (col) {
                col->RecordDone(&sub);
            }
        }
    }

    LOG_INFO(LogGroup::IO, "Found {} matching folders.", matches.size());
    if (matches.empty()) {
        LOG_WARN(LogGroup::IO, "No matching folders found.");
    }

    LOG_INFO(LogGroup::DEFAULT, "--------------------------------------run end-----------------------------------");
    return lastError;
}

int main()
{
    AppOptions options;
    std::string error;
    const std::vector<std::wstring> args = GetCommandLineArgs();

    if (!ParseOptions(args, options, error)) {
        std::cout << "Error: " << error << "\n\n";
        PrintUsage();
        return 2;
    }

    if (options.showHelp) {
        PrintUsage();
        return 0;
    }

    try {
        GlobalInit(options.logger);
        if (options.runTest) {
            return main_test();
        }
        if (!options.extractMp4.empty()) {
            return ExtractDocumentsFromMp4(options.extractMp4, options.extractOutput);
        }
        return RunMux(options);
    }
    catch (const std::exception &e) {
        LOG_CRITICAL(LogGroup::DEFAULT, "Unhandled exception: {}", e.what());
        return 1;
    }
}
