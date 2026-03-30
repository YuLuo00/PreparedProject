
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

#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/mem.h>
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
using PacketBatchQueue = tbb::concurrent_bounded_queue<PacketsBatch>;

const int g_BatchDealPktCount = 120;

static AVPacket *g_EOSPacket = (AVPacket *)1;


int ReadPackets(AVFormatContext *inCtx,
                const std::map<AVFormatContext *, std::map<int, int>> &streamIndexMap,
                PacketBatchQueue &pktsRead);
;

void DealPkts(AVFormatContext *outCtx, PacketBatchQueue &pktsRead);

/**
 * 将输入流 in 的必要信息安全地复制到输出流 out（用于 remux）。
 * 不会复制 priv_data 或内部运行时指针。
 *
 * 返回 0 成功，负值为 AVERROR。
 */
int remux_copy_stream_info(AVStream *in, AVStream *out);


class MediaMux
{
public:
    MediaMux(){};
    std::multimap<AVFormatContext *, AVStream *> GetInputStreams(const std::set<std::string> &files,
                                                                 std::set<AVMediaType> types = {});

    std::multimap<AVFormatContext *, AVStream *> GetInputStreams(const std::string &file,
                                                                 std::set<AVMediaType> types = {});

    int mux(const std::set<std::string> files);
};
