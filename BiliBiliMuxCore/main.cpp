#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

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
void GlobalInit()
{
    Logger::Init();
    LOG_INFO(LogGroup::MUX, "start muxing {}", 123);
    LOG_DEBUG(LogGroup::DECODE, "frame pts={}", 456);
    Logger::SetLevel(LogGroup::MUX, spdlog::level::info);
    Logger::SetLevel(LogGroup::DECODE, spdlog::level::err);
    Logger::SetLevel(LogGroup::FFMPEG, spdlog::level::debug);
    Logger::SetLevel(LogGroup::IO, spdlog::level::debug);

    LOG_DEBUG(LogGroup::MUX, "不会打印");  // 被过滤
    LOG_ERROR(LogGroup::DECODE, "会打印"); // ✔
    av_log_set_callback(my_ffmpeg_log_callback);
}

int mainPipeline(const MediaSubdir &sub)
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

    const std::wstring safeTitle = Tools::sanitize_windows_filename(title);
    const fs::path outputDir = sub.dir.empty() ? fs::current_path() : sub.dir;
    const fs::path outputPath = outputDir / (safeTitle + L".mp4");
    const std::set<std::string> files = {
        Tools::wstring_to_utf8(sub.audio.wstring()),
        Tools::wstring_to_utf8(sub.video.wstring()),
    };

    LOG_INFO(LogGroup::MUX, "开始混流: {}", LOGUTF8(title));
    LOG_INFO(LogGroup::MUX, "输出文件: {}", LOGUTF8(outputPath.filename().wstring()));

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

int main()
{
    GlobalInit();
    //return main_test();
    LOG_INFO(LogGroup::DEFAULT, "--------------------------------------run begin-----------------------------------");


    return add_cover_to_video(R"(C:\Users\Administrator\Desktop\bili_zip_1\type1-1\13369929\【小巫】不行啊♥不故作欢笑是不行的\80\cover.mp4)",
        R"(C:\Users\Administrator\Desktop\bili_zip_1\type1-1\13369929\【小巫】不行啊♥不故作欢笑是不行的\80\【小巫】不行啊♥不故作欢笑是不行的.mp4)",
        R"(C:\Users\Administrator\Desktop\bili_zip_1\type1-1\13369929\【小巫】不行啊♥不故作欢笑是不行的\80\【小巫】不行啊♥不故作欢笑是不行的_cover.jpg)");

    fs::path root = R"(C:\Users\Administrator\Desktop\bili_zip_1)";
    auto matches = BiliCache::CollectBiliFoldersStructured(root);
    MediaSubdir aimSub;
    for (const auto &m : matches) {
        std::string title = BiliCache::GetTitle(m);
        std::wstring titleWstr = Tools::utf8_to_wstring(title);
        LOG_INFO(LogGroup::IO, "{}", LOGUTF8(title));
        for (size_t i = 0; i < m.media_subdirs.size(); i++) {
            MediaSubdir sub = m.media_subdirs[i];
            //mainPipeline(sub);
        }
        if (titleWstr == LR"(【小巫】不行啊♥不故作欢笑是不行的)") {
            if (!m.media_subdirs.empty()) {
                aimSub = m.media_subdirs.front();
            }
            break;
        }
    }

    LOG_INFO(LogGroup::IO, "Found {} matching folders.", matches.size());

    if (aimSub.match == nullptr) {
        LOG_ERROR(LogGroup::MUX, "没有找到目标 MediaSubdir");
        return -1;
    }

        //return -1;
    return mainPipeline(aimSub);
}
