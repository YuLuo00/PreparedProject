#include "ReadNode.h"

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

#include "ffmpegMsg.h"
#include "Logger.h"


int ReadNode::ReadPackets(AVFormatContext *inCtx,
                          const std::map<AVFormatContext *, std::map<int, int>> &streamIndexMap,
                          PacketBatchQueue &pktsRead)

{
    SetThreadDescription(GetCurrentThread(), L"read ");
    m_inCtx = inCtx;
    LOG_INFO(LogGroup::MUX, "{}  >>>>>>>>>> Thread for Read  {}", ::GetCurrentThreadId(), inCtx->url);

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
    int i = 0;
    while (true) {
        //FFmpegLogScope *ffmpegLog = new FFmpegLogScope();

        //auto guard = std::shared_ptr<void>((void *)0x01, [&](void *) {
        //    // 不 delete ffmpegLog，只做你的收尾逻辑
        //    int level = FFmpegLogScope::get_level();
        //    std::string log = FFmpegLogScope::get_log();
        //    this->m_errorLevel.store(std::min(level, this->m_errorLevel.load()));
        //    if (level <= AV_LOG_WARNING) {
        //        //std::cout << "some error happened" << std::endl;
        //    this->m_log.append(log);
        //    }
        //    delete ffmpegLog; // 如果你想 delete，也可以放这里
        //});

        avPacket = av_packet_alloc();
        int ret = 0;
        // 读取
        try {
            i++;
            if (i > 12791) {
                std::cout << "-------" << i << std::endl;
            }
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
    //pktBatches[0].m_pkts.push_back(g_EOSPacket);
    //pktsRead.push(pktBatches[0]);

    LOG_INFO(LogGroup::MUX, " >>>>>>>>>> Finish for Read  {}", inCtx->url);
    return 0;
};