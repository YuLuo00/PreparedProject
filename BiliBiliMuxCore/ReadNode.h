#pragma once


#include <map>
#include <atomic>

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



#include "common.h"

class ReadNode
{
public:
    ReadNode(){};
    std::atomic_bool m_finished = false;
    AVFormatContext *m_inCtx = nullptr;
    std::atomic_int m_errorLevel = AV_LOG_TRACE;
    std::string m_log;
    int ReadPackets(AVFormatContext *inCtx,
                    const std::map<AVFormatContext *, std::map<int, int>> &streamIndexMap,
                    PacketBatchQueue &pktsRead);
};