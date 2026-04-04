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
#include "BiliCache.h"
#include "common.h"
#include "ffmpegMsg.h"
#include "Logger.h"
#include "ReadNode.h"
#include "tools.h"


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

    auto logger = Logger::Get("ffmpeg");
    auto spd_level = FFmpegLevelToSpd(level);

    // 用 spdlog 的 level 控制输出
    if (logger->should_log(spd_level)) {
        logger->log(spd_level, "{}", buf);
    }

    // ❗是否保留 FFmpeg 默认输出（二选一）
    // 一般建议关掉，否则会重复打印
    // av_log_default_callback(ptr, level, fmt, vl);
}


void DealPkts(AVFormatContext *outCtx, PacketBatchQueue &pktsRead)
{
    SetThreadDescription(GetCurrentThread(), L"deal ");
    std::ostringstream oss;
    oss << " >>>>>>>>>> Thread for Deal" << std::this_thread::get_id();
    std::string idStr = oss.str();
    std::cout << idStr << std::endl;

    bool finished = false;
    PacketsBatch pktBatch;
    //int eosCount = 0;
    while (finished == false) {
        pktsRead.pop(pktBatch);
        for (AVPacket *pkt : pktBatch.m_pkts) {
            if (pkt == g_EOSPacket) {
                //eosCount++;
                //if (eosCount >= 2) {
                    finished = true;
                //}
                break;
            }

            if (pkt->flags & AV_PKT_FLAG_KEY) {
            }
            //std::cout << "\t\tpkt->dts" << pkt->dts << "\tpkt->stream_index" << pkt->stream_index << "\tpkt->flags"
            //          << pkt->flags << "\tpkt->size" << pkt->size << "\tpkt->pos" << pkt->pos << std::endl;
            pkt->stream_index = pktBatch.m_outCtxStmIdx;
            int ret = av_interleaved_write_frame(outCtx, pkt);
            if (0 != ret) {
                std::cout << "严重错误，数据包写入失败" << std::endl;
            }
        }
    }
}

/**
 * 将输入流 in 的必要信息安全地复制到输出流 out（用于 remux）。
 * 不会复制 priv_data 或内部运行时指针。
 *
 * 返回 0 成功，负值为 AVERROR。
 */
int remux_copy_stream_info(AVStream *in, AVStream *out)
{
    int ret = 0;

    if (!in || !out) {
        av_log(NULL, AV_LOG_ERROR, "remux_copy_stream_info: null stream pointer\n");
        return AVERROR(EINVAL);
    }

    /* 1) 复制 codec parameters（包含 extradata） */
    ret = avcodec_parameters_copy(out->codecpar, in->codecpar);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "avcodec_parameters_copy failed: %d\n", ret);
        return ret;
    }

    /* 2) 基本字段 */
    out->time_base = in->time_base;
    out->disposition = in->disposition;
    out->start_time = in->start_time;
    out->duration = in->duration;

    /* 3) 帧率/采样比 */
    out->avg_frame_rate = in->avg_frame_rate;
    out->r_frame_rate = in->r_frame_rate;
    out->sample_aspect_ratio = in->sample_aspect_ratio;

    /* 4) 复制 metadata（如果有） */
    if (in->metadata) {
        ret = av_dict_copy(&out->metadata, in->metadata, 0);
        if (ret < 0) {
            av_log(NULL, AV_LOG_WARNING, "av_dict_copy failed: %d\n", ret);
            /* metadata 复制失败不一定致命，继续执行 */
        }
    }

    /* 5) 复制 attached_pic（如果存在） */
    if (in->attached_pic.size > 0) {
        /* 确保目标没有残留 */
        av_packet_unref(&out->attached_pic);
        ret = av_packet_ref(&out->attached_pic, &in->attached_pic);
        if (ret < 0) {
            av_log(NULL, AV_LOG_WARNING, "av_packet_ref for attached_pic failed: %d\n", ret);
            /* 非致命，继续 */
        }
    }

    /* 6) 复制 side_data（逐项分配并添加） */
    if (in->nb_side_data > 0 && in->side_data) {
        for (int i = 0; i < in->nb_side_data; i++) {
            AVPacketSideData *sd = &in->side_data[i];
            if (!sd || sd->size <= 0)
                continue;

            uint8_t *buf = (uint8_t *)av_malloc(sd->size);
            if (!buf) {
                av_log(NULL, AV_LOG_WARNING, "av_malloc failed for side_data size %d\n", sd->size);
                ret = AVERROR(ENOMEM);
                break;
            }
            memcpy(buf, sd->data, sd->size);

            /* av_stream_add_side_data 会把 buf 作为 side data 的所有者 */
            if (!av_stream_add_side_data(out, sd->type, buf, sd->size)) {
                /* av_stream_add_side_data 返回 NULL 表示失败（或内存问题） */
                av_log(NULL, AV_LOG_WARNING, "av_stream_add_side_data failed for type %d\n", sd->type);
                av_free(buf);
                /* 继续尝试复制其他 side data */
            }
        }
    }

    /* 7) 其他不应复制的字段：priv_data、codec、internal 等均不处理 */

    return ret >= 0 ? 0 : ret;
}

class MediaMux
{
public:
    MediaMux(){};
    static std::multimap<AVFormatContext *, AVStream *> GetInputStreams(const std::set<std::string> &files,
                                                                 std::set<AVMediaType> types = {})
    {
        std::multimap<AVFormatContext *, AVStream *> result;

        for (const auto &file : files) {
            // 调用单文件版本
            auto one = GetInputStreams(file, types);

            // 合并到 result
            result.insert(one.begin(), one.end());
        }

        return result;
    }

    static std::multimap<AVFormatContext *, AVStream *> GetInputStreams(const std::string &file,
                                                                 std::set<AVMediaType> types = {})
    {
        std::multimap<AVFormatContext *, AVStream *> result;
        // 读取视频文件
        char errors[1024];
        AVFormatContext *inCtx = nullptr;
        int ret = 0;
        ret = avformat_open_input(&inCtx, file.c_str(), NULL, NULL);
        if (inCtx == nullptr) {
            std::cout << "严重错误，open失败" << std::endl;
            av_strerror(ret, errors, strlen(errors));
            av_log(NULL, AV_LOG_WARNING, "error, ret=%d, msg=%s\n", ret, errors);
            return result;
        }
        if (ret != 0) {
            av_strerror(ret, errors, strlen(errors));
            av_log(NULL, AV_LOG_WARNING, "error, ret=%d, msg=%s\n", ret, errors);
            return result;
        }
        // 检查 流信息
        avformat_find_stream_info(inCtx, NULL);
        for (int i = 0; i < inCtx->nb_streams; i++) {
            // 遍历媒体流
            AVStream *inputStream = inCtx->streams[i];
            // 按类型过滤
            AVMediaType type = inputStream->codecpar->codec_type;
            if (types.empty() == false && types.count(type) == 0) {
                continue;
            }

            result.insert({inCtx, inputStream});
        }
        return result;
    }

    
    AVFormatContext *m_outCtx = nullptr;
    std::set<std::string> m_files;
    std::multimap<AVFormatContext *, AVStream *> inputStreams;
    int Open(const std::set<std::string> files)
    {
        m_files = files;
        int ret = 0;
        inputStreams = GetInputStreams(m_files);

        // 构建输出文件
        char errMsg[AV_ERROR_MAX_STRING_SIZE] = {'\0'};
        int error = avformat_alloc_output_context2(&m_outCtx, nullptr, nullptr, "result.mp4");
        if (error < 0) {
            // 输出错误代码及错误信息
            av_make_error_string(errMsg, AV_ERROR_MAX_STRING_SIZE, error);
            std::cout << "Failed to allocate output format context: " << errMsg << std::endl;
            // 处理错误情况
            return -2;
        }
        return ret;
    }
    void Close()
    {
        if (m_outCtx) {
            int ret = av_write_trailer(m_outCtx);
            if (ret != 0) {
                char errMsg[AV_ERROR_MAX_STRING_SIZE] = {'\0'};
                av_strerror(ret, errMsg, 200);
                av_log(NULL, AV_LOG_WARNING, "av_write_trailer error: ret=%d, msg=%s\n", ret, errMsg);
            }
            avio_close(m_outCtx->pb);
            m_outCtx = nullptr;
        }
    }

    int mux(bool autoCloseOutput = true)
    {
        int ret = 0;


        // 构建输出流， 记录映射关系
        std::map<AVFormatContext *, std::map<int, int>>
            streamIndexMap; // <inputCtx, <InputCtxStmIndex, outputCtxStmIdx>>
        for (const auto &p : inputStreams) {
            AVFormatContext *inCtx = p.first;
            AVStream *inputStream = p.second;
            // 复制到输出流
            AVStream *newStream = avformat_new_stream(m_outCtx, NULL);
            remux_copy_stream_info(inputStream, newStream);
            // 记录流映射
            if (streamIndexMap[inCtx].count(inputStream->index)) {
                av_log(NULL, AV_LOG_ERROR, "error, 重复映射了同一个输入流\n");
            }
            streamIndexMap[inCtx][inputStream->index] = newStream->index;
        }

        // 打开文件,写入文件头
        avio_open(&m_outCtx->pb, "result.mp4", AVIO_FLAG_WRITE);
        ret = avformat_write_header(m_outCtx, NULL);
        if (ret != 0) {
            return -2;
        }


        tbb::flow::graph g;
        // broadcast_node 触发读取和处理
        tbb::flow::broadcast_node<PacketBatchQueue *> start(g);

        // 读取节点
        std::vector<AVFormatContext *> ctsx;
        for (auto it = inputStreams.begin(); it != inputStreams.end(); it = inputStreams.upper_bound(it->first)) {
            AVFormatContext *inCtx = it->first;
            // 处理这个 key
            std::cout << "inCtx = " << inCtx << std::endl;
            auto range = inputStreams.equal_range(inCtx);
            ctsx.push_back(inCtx);

        }
        std::vector<ReadNode> readTasks(ctsx.size());
        {
            using namespace oneapi::tbb::flow;

            graph g;

            using PktsPtr = PacketBatchQueue *;

            broadcast_node<PktsPtr> start(g);
            // read 节点数组（必须保证节点对象本身不被移动）
            std::vector<std::unique_ptr<function_node<PktsPtr, int>>> readNodes;

            PacketBatchQueue pktsRead;
            std::atomic<int> counter = 0;
            // 计数节点：每来一个输入就 ++counter
            function_node<int, continue_msg> counter_node(g,
                                                            serial, // 串行保证计数安全
                                                            [&](int) -> continue_msg {
                                                                counter.fetch_add(1);
                                                                if (counter.load() >= readNodes.size()) {
                                                                    PacketsBatch eosBatch;
                                                                    eosBatch.m_pkts.push_back(g_EOSPacket);
                                                                    pktsRead.push(eosBatch);
                                                                }
                                                                return continue_msg{}; // 始终输出一个信号
                                                            });
            // read 节点
            for (int i = 0; i < ctsx.size(); ++i) {
                auto readNode =
                    std::make_unique<function_node<PktsPtr, int>>(g, unlimited, [&, i](PktsPtr pktsPtr) -> int {
                        PacketBatchQueue &pkts = *pktsPtr;
                        return readTasks[i].ReadPackets(ctsx[i], streamIndexMap, pkts); // ✔ 返回 int
                        //return ReadPackets(ctsx[i], streamIndexMap, pkts); // ✔ 返回 int
                    });

                make_edge(start, *readNode);
                make_edge(*readNode, counter_node);
                readNodes.push_back(std::move(readNode));
            }

            // deal 节点
            function_node<PktsPtr> deal(g, unlimited, [&](PktsPtr pktsPtr) {
                PacketBatchQueue &pkts = *pktsPtr;
                DealPkts(m_outCtx, pkts);
            });
            make_edge(start, deal);

            // 启动
            pktsRead.set_capacity(10); // 队列最大容量
            start.try_put(&pktsRead);

            g.wait_for_all();
        }

        //return 0;
        // 写入文件尾部
        // 关闭文件
        if (autoCloseOutput) {
            Close();
        }
        for (size_t i = 0; i < ctsx.size(); i++) {
            AVFormatContext *ctx = ctsx[i];
            //avio_close(ctx->pb);
            avformat_close_input(&ctx);
        }

        std::cout << "All done!" << std::endl;
        return 0;
    }
};




void GlobalInit()
{
    Logger::Init();
    LOG_INFO("mux", "start muxing {}", 123);
    LOG_DEBUG("decode", "frame pts={}", 456);
    Logger::SetLevel("mux", spdlog::level::info);
    Logger::SetLevel("decode", spdlog::level::err);
    Logger::SetLevel("ffmpeg", spdlog::level::debug);

    LOG_DEBUG("mux", "不会打印");  // 被过滤
    LOG_ERROR("decode", "会打印"); // ✔
    av_log_set_callback(my_ffmpeg_log_callback);
}

int main()
{
    GlobalInit();


    fs::path root = R"(C:\Users\Administrator\Desktop\bili_zip_1)";
    auto matches = BiliCache::CollectBiliFoldersStructured(root);
    Match aimMatch;
    std::vector<MediaInfo> infos;
    for (const auto &m : matches) {
        std::string title = BiliCache::GetTitle(m);
        std::wstring titleWstr = Tools::utf8_to_wstring(title);
        std::cout << Tools::Utf8ToLocal(title) << std::endl;
        if (titleWstr == LR"(夏日再见∪人见人爱小海豚~)") {
            aimMatch = m;
            //break;
        }
        for (const MediaSubdir &sub : m.media_subdirs) {
            MediaInfo mediaInfo;
            loadFile2MediaInfo(m.entry_json_path.generic_wstring(), mediaInfo);
            loadFile2MediaInfo(sub.index.generic_wstring(), mediaInfo);
            infos.push_back(mediaInfo);
        }
        
    }

    std::cout << "Found " << matches.size() << " matching folders.\n";

    ////AvApiWrapper::_AvformatAllocOutputContext2()

    //return 0;
    //


    MediaMux mux;
    //av_log_set_callback(my_ffmpeg_log_callback);
    //av_log_set_level(AV_LOG_VERBOSE); // 或 AV_LOG_DEBUG

    
    const std::set<std::string> files = {
        R"(C:\Users\Administrator\Desktop\bili_zip_1\00\285915854\1\120\audio.m4s)",
        R"(C:\Users\Administrator\Desktop\bili_zip_1\00\285915854\1\120\video.m4s)",

        //R"(C:\Users\Administrator\Desktop\bili_zip_1\type1-1\c_469842584\120\audio.m4s)",
        //R"(C:\Users\Administrator\Desktop\bili_zip_1\type1-1\c_469842584\120\video.m4s)",
    };


    mux.Open(files);
    write_mediainfo_to_avformat(mux.m_outCtx, infos[0]);
    mux.mux(false);
    mux.Close();

    
    AVFormatContext *ctx = nullptr;
    ctx = mux.m_outCtx;
    ////int i = av_dict_count(mux.m_outCtx->);

    std::multimap<AVFormatContext *, AVStream *> ii = MediaMux::GetInputStreams("result.mp4");
    ctx = ii.begin()->first;
    MediaInfo media;
    read_mediainfo_from_avformat(ctx, media);
    print_avformat_metadata(ctx);
    return -1;
    return 0;
}
