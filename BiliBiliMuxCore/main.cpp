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
using namespace std;

#include <iostream>
#include <tbb/concurrent_queue.h>
#include <tbb/flow_graph.h>

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

class LogDebug
{
public:
    LogDebug(){};

    template <class T> LogDebug operator<<(T t)
    {
        log(t);
        return *this;
    };

    template <class T> void log(T t)
    {
        cout << t;
    };

private:
    function<void(void *)> m_logCb = nullptr;
};

void ReadFileTTTT(const string vedioPath,
                  AVFormatContext *outCtx,
                  AVMediaType type,
                  map<int, pair<vector<AVPacket *>, AVRational>> &allPackets)
{
    set<int> dtsDedupCheck;
    map<int, int> streamsIdx; // <输入流流序号，输出流流序号>
    char errors[1024];
    AVFormatContext *inCtx = nullptr;
    int ret = 0;
    // 读取视频文件
    ret = avformat_open_input(&inCtx, vedioPath.c_str(), NULL, NULL);
    if (inCtx == nullptr) {
        std::cout << "严重错误，open失败" << std::endl;
        return;
    }
    if (ret != 0) {
        av_strerror(ret, errors, strlen(errors));
        av_log(NULL, AV_LOG_WARNING, "error, ret=%d, msg=%s\n", ret, errors);
        return;
    }
    avformat_find_stream_info(inCtx, NULL);
    for (int i = 0; i < inCtx->nb_streams; i++) {
        // 遍历指定媒体流
        AVStream *inputStream = inCtx->streams[i];
        int inIdx = inputStream->index;
        if (inputStream->codecpar->codec_type == type) {
            // 复制到输出流
            AVStream *newStream = avformat_new_stream(outCtx, NULL);
            avcodec_parameters_copy(newStream->codecpar, inCtx->streams[i]->codecpar);
            // 记录索引
            streamsIdx[i] = newStream->index;
            if (allPackets.count(newStream->index)) {
                std::cout << "严重错误，输出流重复" << std::endl;
                exit(-1);
            }
            // 录入丢失的流数据字段
            newStream->time_base = inputStream->time_base;
            allPackets[newStream->index].second = newStream->time_base;
        }
    }
    int sizeBuff = 0;
    int index = 0;
    // 读取一段数据包
    while (true) {
        AVPacket *avPacket = av_packet_alloc();
        int ret = 0;
        // 读取
        try {
            ret = av_read_frame(inCtx, avPacket);
            if (ret < 0) {
                std::cout << "读取数据包错误" << std::endl;
                av_packet_free(&avPacket);
                break;
            }
            if (avPacket->dts < 0) {
                std::cout << "解码时间戳小于0" << std::endl;
            }
            if (avPacket->size <= 0) {
                std::cout << "读取文件结束" << std::endl;
                av_packet_free(&avPacket);
                break;
            }
        }
        catch (...) {
            std::cout << "触发了异常" << std::endl;
        }
        // 校验序号
        if (streamsIdx.count(avPacket->stream_index) < 0) {
            av_packet_free(&avPacket);
            continue;
        }
        // 缓存
        if (dtsDedupCheck.count(avPacket->dts) > 0) {
            av_packet_free(&avPacket);
            continue;
        }
        dtsDedupCheck.insert(avPacket->dts);
        int outStreamIdx = streamsIdx[avPacket->stream_index];
        allPackets[outStreamIdx].first.push_back(avPacket);
    }
};

#include <chrono>
#include <fstream>
#include <iostream>
#include <tbb/flow_graph.h>
#include <thread>

int mainCheckDoubleRead(std::string vedioPath)
{
    char errors[1024];
    AVFormatContext *inCtx = nullptr;
    int ret = 0;
    // 读取视频文件
    ret = avformat_open_input(&inCtx, vedioPath.c_str(), NULL, NULL);
    if (inCtx == nullptr) {
        std::cout << "严重错误，open失败" << std::endl;
        return 1;
    }
    if (ret != 0) {
        av_strerror(ret, errors, strlen(errors));
        av_log(NULL, AV_LOG_WARNING, "error, ret=%d, msg=%s\n", ret, errors);
        return 1;
    }

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

    // 检查 && 复制 流信息
    avformat_find_stream_info(inCtx, NULL);
    std::map<AVFormatContext *, std::map<int, int>> streamIndexMap; // <inputCtx, <InputCtxStmIndex, outputCtxStmIdx>>
    std::vector<AVMediaType> streamsType(inCtx->nb_streams, AVMediaType::AVMEDIA_TYPE_UNKNOWN);
    for (int i = 0; i < inCtx->nb_streams; i++) {
        // 遍历媒体流
        AVStream *inputStream = inCtx->streams[i];
        int inIdx = inputStream->index;
        streamsType[i] = inputStream->codecpar->codec_type;

        // 复制到输出流
        AVStream *newStream = avformat_new_stream(outCtx, NULL);
        avcodec_parameters_copy(newStream->codecpar, inputStream->codecpar);

        // 录入丢失的流数据字段
        newStream->time_base = inputStream->time_base;

        // 记录流映射
        if (streamIndexMap[inCtx].count(inIdx)) {
            av_log(NULL, AV_LOG_ERROR, "error, 重复映射了同一个输入流\n");
        }
        streamIndexMap[inCtx][inIdx] = newStream->index;
    }


    tbb::flow::graph g;

    //// 打开一个文件（共享）
    // std::ifstream file(path, std::ios::binary);
    // if (!file.is_open()) {
    //     std::cerr << "Failed to open file!" << std::endl;
    //     return -1;
    // }

    //AVFormatContext *inCtx = nullptr;
    //int ret = 0;
    // 读取视频文件
    ret = avformat_open_input(&inCtx, vedioPath.c_str(), NULL, NULL);
    if (inCtx == nullptr) {
        std::cout << "严重错误，open失败" << std::endl;
        return 2;
    }

    // 第一个节点
    tbb::flow::function_node<int, int> node1(g,
                                             tbb::flow::serial, // 单线程处理
                                             [&](int token) -> int {
                                                 for (int i = 0; i < 10; ++i) {
                                                     std::this_thread::sleep_for(std::chrono::seconds(1));
                                                     std::cout << "Node1 iteration " << i + 1 << std::endl;
                                                 }
                                                 return 0;
                                             });

    // 第二个节点
    tbb::flow::function_node<int, int> node2(g, tbb::flow::serial, [&](int token) -> int {
        for (int i = 0; i < 10; ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            std::cout << "Node2 iteration " << i + 1 << std::endl;
        }
        return 0;
    });

    // 可以用 a broadcast_node 给两个节点发信号启动
    tbb::flow::broadcast_node<int> start(g);
    tbb::flow::make_edge(start, node1);
    tbb::flow::make_edge(start, node2);

    // 发一个信号开始执行
    start.try_put(0);

    g.wait_for_all();
    std::cout << "All nodes finished!" << std::endl;
    return 0;
}

void ReadPackets(AVFormatContext *inCtx,
    const std::map<AVFormatContext *, std::map<int, int>> &streamIndexMap,
    PacketBatchQueue &pktsRead)
{
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
            if (ret < 0) {
                std::cout << "读取数据包错误" << std::endl;
                av_packet_free(&avPacket);
                break;
            }
            if (avPacket->dts < 0) {
                std::cout << "解码时间戳小于0" << std::endl;
            }
            if (avPacket->size <= 0) {
                std::cout << "读取文件结束" << std::endl;
                av_packet_free(&avPacket);
                break;
            }
            PacketsBatch &pktBatch = pktBatches[avPacket->stream_index];
            pktBatch.m_pkts.push_back(avPacket);
            if (pktBatch.m_pkts.size() >= g_BatchDealPktCount) {
                pktsRead.push(pktBatch);
                pktBatch.m_pkts.clear();
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
////std::map<AVFormatContext *, std::map<int, int>> streamIndexMa;
//void ReadPackets(const string vedioPath,
//    PacketBatchQueue &pktsRead)
//{
//    set<int> dtsDedupCheck;
//    map<int, int> streamsIdx; // <输入流流序号，输出流流序号>
//    char errors[1024];
//    AVFormatContext *inCtx = nullptr;
//    int ret = 0;
//
//    std::vector<AVPacket *> buffer;
//    // 读取视频文件
//    ret = avformat_open_input(&inCtx, vedioPath.c_str(), NULL, NULL);
//    if (inCtx == nullptr) {
//        std::cout << "严重错误，open失败" << std::endl;
//        return;
//    }
//    if (ret != 0) {
//        av_strerror(ret, errors, strlen(errors));
//        av_log(NULL, AV_LOG_WARNING, "error, ret=%d, msg=%s\n", ret, errors);
//        return;
//    }
//
//    return ReadPackets(inCtx, pktsRead);
//}

void DealPkts(AVFormatContext *outCtx, PacketBatchQueue &pktsRead)
{
    std::ostringstream oss;
    oss << " >>>>>>>>>> Thread for Deal" << std::this_thread::get_id();
    std::string idStr = oss.str();
    std::cout << idStr << std::endl;

    bool finished = false;
    PacketsBatch pktBatch;
    while (finished == false) {
        pktsRead.pop(pktBatch);
        for (AVPacket *pkt : pktBatch.m_pkts) {
            if (pkt == g_EOSPacket) {
                finished = true;
                break;
            }

            if (pkt->flags & AV_PKT_FLAG_KEY) {
            }
            std::cout << "\t\tpkt->dts" << pkt->dts << "\tpkt->stream_index" << pkt->stream_index << "\tpkt->flags"
                      << pkt->flags << "\tpkt->size" << pkt->size << "\tpkt->pos" << pkt->pos << std::endl;
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
    MediaMux mux;
    int ret = 0;

    // 读取输入文件
    std::multimap<AVFormatContext *, AVStream *> inputStreams = mux.GetInputStreams(vedioPath);

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
    //for (const auto &p : inputStreams) {
    
        AVFormatContext *inCtx = inputStreams.begin()->first;
        AVStream *inputStream = inputStreams.begin()->second;
        tbb::flow::function_node<PacketBatchQueue *, tbb::flow::continue_msg> readPktsNode(
            g, tbb::flow::serial, [&](PacketBatchQueue *queue) -> tbb::flow::continue_msg {
                ReadPackets(inCtx, streamIndexMap, *queue);
                return tbb::flow::continue_msg();
            });
        //auto readPktsNode = new tbb::flow::function_node<PacketBatchQueue *, tbb::flow::continue_msg>(
        //    g, tbb::flow::serial, [&](PacketBatchQueue *queue) -> tbb::flow::continue_msg {
        //        ReadPackets(inCtx, streamIndexMap, *queue);
        //        return tbb::flow::continue_msg();
        //    });
        tbb::flow::make_edge(start, readPktsNode);
    //}


    // DealPkts 节点，处理队列中的包
    tbb::flow::function_node<PacketBatchQueue *, tbb::flow::continue_msg> dealPktsNode(
        g, tbb::flow::serial, [&outCtx](PacketBatchQueue *queue) -> tbb::flow::continue_msg {
            DealPkts(outCtx, *queue);
            return tbb::flow::continue_msg();
        });


    tbb::flow::make_edge(start, dealPktsNode);

    // 触发一次
    start.try_put(&pktsRead);

    g.wait_for_all();

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