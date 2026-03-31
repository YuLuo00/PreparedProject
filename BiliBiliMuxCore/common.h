#pragma once

#include <vector>
#include <map>

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

#include <tbb/concurrent_queue.h>
#include <tbb/flow_graph.h>
using namespace tbb::flow;


const int g_BatchDealPktCount = 120;


extern AVPacket *g_EOSPacket;



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