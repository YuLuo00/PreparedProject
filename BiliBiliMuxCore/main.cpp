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

#include <tbb/concurrent_queue.h>
#include <tbb/flow_graph.h>




using namespace tbb::flow;

class PacketsBatch
{
public:
    PacketsBatch(){};
    std::vector<AVPacket *> m_pkts;
    AVFormatContext *m_inCtx = nullptr;
    int m_inCtxStmIdx = 0;
    int m_outCtxStmIdx = 0;
};
using PacketBatchQueue=tbb::concurrent_bounded_queue<PacketsBatch>;

const int g_BatchDealPktCount = 120;

AVPacket *g_EOSPacket = (AVPacket *)1;



void ReadPackets(AVFormatContext *inCtx,
    const std::map<AVFormatContext *, std::map<int, int>> &streamIndexMap,
    PacketBatchQueue &pktsRead)
{
    SetThreadDescription(GetCurrentThread(), L"read ");
    std::ostringstream oss;
    oss << " >>>>>>>>>> Thread for Read" << std::this_thread::get_id();
    std::string idStr = oss.str();
    std::cout << idStr << std::endl;

    set<int> dtsDedupCheck;
    map<int, int> streamsIdx; // <输入流流序号，输出流流序号>
    char errors[1024];
    int ret = 0;

    std::vector<AVPacket *> buffer;
    //avformat_find_stream_info(inCtx, NULL);
    std::vector<PacketsBatch> pktBatches(inCtx->nb_streams);
    std::set<int> vedioStreamIdx;
    auto isVedioPacket = [inCtx](const AVPacket *pkt) {
        return inCtx->streams[pkt->stream_index]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO;
    };
    for (size_t i = 0; i < inCtx->nb_streams; i++) {
        pktBatches[i].m_inCtx = inCtx;
        pktBatches[i].m_inCtxStmIdx = i;
        pktBatches[i].m_outCtxStmIdx = streamIndexMap.at(inCtx).at(i);
    }

    // 读取一段数据包
    AVPacket *avPacket = nullptr;
    while (true) {
        avPacket = av_packet_alloc();
        int ret = 0;
        // 读取
        try {
            ret = av_read_frame(inCtx, avPacket);
            if (ret == AVERROR_EOF) {
                std::cout << "读取文件结束" << std::endl;
                av_packet_free(&avPacket);
                break;
            }
            if (ret < 0) {
                char errbuf[256];
                av_strerror(ret, errbuf, sizeof(errbuf));
                std::cout << "读取数据包错误: " << errbuf << std::endl;

                av_packet_free(&avPacket);
                break;
            }
            if (avPacket->dts < 0) {
                std::cout << "解码时间戳小于0" << std::endl;
            }
            PacketsBatch &pktBatch = pktBatches[avPacket->stream_index];
            pktBatch.m_pkts.push_back(avPacket);
            bool isIFrame = (avPacket->flags & AV_PKT_FLAG_KEY) != 0;
            //if (pktBatch.m_pkts.size() >= g_BatchDealPktCount && (!isVedioPacket(avPacket) || isIFrame)) {
            //    pktsRead.push(pktBatch);
            //    pktBatch.m_pkts.clear();
            //}
            if (pktBatch.m_pkts.size() >= g_BatchDealPktCount) {
                if (isVedioPacket(avPacket)) {
                    if (isIFrame) {
                        pktsRead.push(pktBatch);
                        pktBatch.m_pkts.clear();
                    }
                    else {
                        //std::cout << "等待I帧" << std::endl;
                    }
                }
                else {
                    pktsRead.push(pktBatch);
                    pktBatch.m_pkts.clear();
                }
            }
        }
        catch (...) {
            std::cout << "触发了异常" << std::endl;
        }
    }
    for (size_t i = 0; i < pktBatches.size(); i++) {
        PacketsBatch &pktBatch = pktBatches[i];
        if (pktBatch.m_pkts.empty()) {
            continue;
        }
        pktsRead.push(pktBatch);
        pktBatch.m_pkts.clear();
    }
    pktBatches[0].m_pkts.push_back(g_EOSPacket);
    pktsRead.push(pktBatches[0]);
};

void DealPkts(AVFormatContext *outCtx, PacketBatchQueue &pktsRead)
{
    SetThreadDescription(GetCurrentThread(), L"deal ");
    std::ostringstream oss;
    oss << " >>>>>>>>>> Thread for Deal" << std::this_thread::get_id();
    std::string idStr = oss.str();
    std::cout << idStr << std::endl;

    bool finished = false;
    PacketsBatch pktBatch;
    int eosCount = 0;
    while (finished == false) {
        pktsRead.pop(pktBatch);
        for (AVPacket *pkt : pktBatch.m_pkts) {
            if (pkt == g_EOSPacket) {
                eosCount++;
                if (eosCount >= 2) {
                    finished = true;
                }
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


#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/mem.h>

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
    std::multimap<AVFormatContext *, AVStream *> GetInputStreams(const std::set<std::string> &files,
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

    std::multimap<AVFormatContext *, AVStream *> GetInputStreams(const std::string &file,
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
};

int main()
{
    std::string vedioPath = R"(C:\Users\Administrator\Desktop\BiliBiliMux\bin\qingziTU.mp4)";
    vedioPath = R"()";
    vedioPath = R"()";
    MediaMux mux;
    int ret = 0;
    
    
    const std::set<std::string> files = {
           R"(C:\Users\Administrator\Desktop\bili_zip_1\10723293\1\80\audio.m4s)",
           R"(C:\Users\Administrator\Desktop\bili_zip_1\10723293\1\80\video.m4s)",
    };
    // 读取输入文件
    //std::multimap<AVFormatContext *, AVStream *> inputStreams = mux.GetInputStreams(vedioPath);
    std::multimap<AVFormatContext *, AVStream *> inputStreams = mux.GetInputStreams(files);

    // 构建输出文件
    AVFormatContext *outCtx = nullptr;
    char errMsg[AV_ERROR_MAX_STRING_SIZE] = {'\0'};
    int error = avformat_alloc_output_context2(&outCtx, nullptr, nullptr, "result.mp4");
    if (error < 0) {
        // 输出错误代码及错误信息
        av_make_error_string(errMsg, AV_ERROR_MAX_STRING_SIZE, error);
        std::cout << "Failed to allocate output format context: " << errMsg << std::endl;
        // 处理错误情况
        return -2;
    }
    // 构建输出流， 记录映射关系
    std::map<AVFormatContext *, std::map<int, int>> streamIndexMap; // <inputCtx, <InputCtxStmIndex, outputCtxStmIdx>>
    for (const auto &p : inputStreams) {
        AVFormatContext *inCtx = p.first;
        AVStream *inputStream = p.second;
        // 复制到输出流
        AVStream *newStream = avformat_new_stream(outCtx, NULL);
        remux_copy_stream_info(inputStream, newStream);
        // 记录流映射
        if (streamIndexMap[inCtx].count(inputStream->index)) {
            av_log(NULL, AV_LOG_ERROR, "error, 重复映射了同一个输入流\n");
        }
        streamIndexMap[inCtx][inputStream->index] = newStream->index;
    }

    // 打开文件,写入文件头
    avio_open(&outCtx->pb, "result.mp4", AVIO_FLAG_WRITE);
    ret = avformat_write_header(outCtx, NULL);
    if (ret != 0) {
        return -2;
    }

    PacketBatchQueue pktsRead;
    pktsRead.set_capacity(10); // 队列最大容量

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

    bool testSyncRead = true;
    testSyncRead = false;
    if (testSyncRead) // 测试异步读取两个文件
    {
        // 启动两个线程
        std::thread t3([&] { DealPkts(outCtx, pktsRead); });

        // 等待两个线程执行完毕
        std::thread t1([&] { ReadPackets(ctsx[0], streamIndexMap, pktsRead); });
        t1.join();
        std::thread t2([&] { ReadPackets(ctsx[1], streamIndexMap, pktsRead); });
        t2.join();

        t3.join();
    }
    else {
        {
            {
                using namespace oneapi::tbb::flow;

                graph g;

                using PktsPtr = PacketBatchQueue *;

                broadcast_node<PktsPtr> start(g);

                // read 节点数组（必须保证节点对象本身不被移动）
                std::vector<std::unique_ptr<function_node<PktsPtr>>> readNodes;
                readNodes.reserve(ctsx.size());

                for (int i = 0; i < ctsx.size(); ++i) {
                    auto node = std::make_unique<function_node<PktsPtr>>(g, unlimited, [&, i](PktsPtr pktsPtr) {
                        PacketBatchQueue &pkts = *pktsPtr;
                        ReadPackets(ctsx[i], streamIndexMap, pkts);
                    });

                    make_edge(start, *node);
                    readNodes.push_back(std::move(node));
                }

                // deal 节点
                function_node<PktsPtr> deal(g, unlimited, [&](PktsPtr pktsPtr) {
                    PacketBatchQueue &pkts = *pktsPtr;
                    DealPkts(outCtx, pkts);
                });

                make_edge(start, deal);

                // 启动
                start.try_put(&pktsRead);

                g.wait_for_all();
            }
        }
    }

    //return 0;
    	// 写入文件尾部
    ret = av_write_trailer(outCtx);
    if (ret != 0) {
        av_strerror(ret, errMsg, 200);
        av_log(NULL, AV_LOG_WARNING, "av_write_trailer error: ret=%d, msg=%s\n", ret, errMsg);
    }
    // 关闭文件
    avio_close(outCtx->pb);
    //if (inCtx) {
    //    avformat_close_input(&inCtx); // 会自动关闭内部的 AVIOContext
    //}

    std::cout << "All done!" << std::endl;
    return 0;
}