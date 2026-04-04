#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "MediaMux.h"

#include <atomic>
#include <cstring>
#include <memory>
#include <sstream>
#include <thread>
#include <vector>

#include <libavutil/mem.h>

#include "Logger.h"
#include "ReadNode.h"

namespace
{
void DealPkts(AVFormatContext *outCtx, PacketBatchQueue &pktsRead)
{
    SetThreadDescription(GetCurrentThread(), L"deal ");
    std::ostringstream oss;
    oss << " >>>>>>>>>> Thread for Deal" << std::this_thread::get_id();
    LOG_INFO(LogGroup::MUX, "{}", oss.str());

    bool finished = false;
    PacketsBatch pktBatch;
    while (!finished) {
        pktsRead.pop(pktBatch);
        for (AVPacket *pkt : pktBatch.m_pkts) {
            if (pkt == g_EOSPacket) {
                finished = true;
                break;
            }

            pkt->stream_index = pktBatch.m_outCtxStmIdx;
            const int ret = av_interleaved_write_frame(outCtx, pkt);
            if (ret != 0) {
                LOG_ERROR(LogGroup::MUX, "严重错误，数据包写入失败");
            }
        }
    }
}

int remux_copy_stream_info(AVStream *in, AVStream *out)
{
    int ret = 0;

    if (!in || !out) {
        av_log(nullptr, AV_LOG_ERROR, "remux_copy_stream_info: null stream pointer\n");
        return AVERROR(EINVAL);
    }

    ret = avcodec_parameters_copy(out->codecpar, in->codecpar);
    if (ret < 0) {
        av_log(nullptr, AV_LOG_ERROR, "avcodec_parameters_copy failed: %d\n", ret);
        return ret;
    }

    out->time_base = in->time_base;
    out->disposition = in->disposition;
    out->start_time = in->start_time;
    out->duration = in->duration;

    out->avg_frame_rate = in->avg_frame_rate;
    out->r_frame_rate = in->r_frame_rate;
    out->sample_aspect_ratio = in->sample_aspect_ratio;

    if (in->metadata) {
        ret = av_dict_copy(&out->metadata, in->metadata, 0);
        if (ret < 0) {
            av_log(nullptr, AV_LOG_WARNING, "av_dict_copy failed: %d\n", ret);
        }
    }

    if (in->attached_pic.size > 0) {
        av_packet_unref(&out->attached_pic);
        ret = av_packet_ref(&out->attached_pic, &in->attached_pic);
        if (ret < 0) {
            av_log(nullptr, AV_LOG_WARNING, "av_packet_ref for attached_pic failed: %d\n", ret);
        }
    }

    if (in->nb_side_data > 0 && in->side_data) {
        for (int i = 0; i < in->nb_side_data; i++) {
            AVPacketSideData *sd = &in->side_data[i];
            if (!sd || sd->size <= 0) {
                continue;
            }

            uint8_t *buf = static_cast<uint8_t *>(av_malloc(sd->size));
            if (!buf) {
                av_log(nullptr, AV_LOG_WARNING, "av_malloc failed for side_data size %d\n", sd->size);
                ret = AVERROR(ENOMEM);
                break;
            }

            memcpy(buf, sd->data, sd->size);
            if (!av_stream_add_side_data(out, sd->type, buf, sd->size)) {
                av_log(nullptr, AV_LOG_WARNING, "av_stream_add_side_data failed for type %d\n", sd->type);
                av_free(buf);
            }
        }
    }

    return ret >= 0 ? 0 : ret;
}
} // namespace

std::multimap<AVFormatContext *, AVStream *> MediaMux::GetInputStreams(const std::set<std::string> &files,
                                                                       std::set<AVMediaType> types)
{
    std::multimap<AVFormatContext *, AVStream *> result;

    for (const auto &file : files) {
        auto one = GetInputStreams(file, types);
        result.insert(one.begin(), one.end());
    }

    return result;
}

std::multimap<AVFormatContext *, AVStream *> MediaMux::GetInputStreams(const std::string &file,
                                                                       std::set<AVMediaType> types)
{
    std::multimap<AVFormatContext *, AVStream *> result;
    char errors[1024] = {'\0'};
    AVFormatContext *inCtx = nullptr;
    int ret = avformat_open_input(&inCtx, file.c_str(), nullptr, nullptr);
    if (inCtx == nullptr) {
        LOG_ERROR(LogGroup::IO, "严重错误，open失败");
        av_strerror(ret, errors, sizeof(errors));
        av_log(nullptr, AV_LOG_WARNING, "error, ret=%d, msg=%s\n", ret, errors);
        return result;
    }
    if (ret != 0) {
        av_strerror(ret, errors, sizeof(errors));
        av_log(nullptr, AV_LOG_WARNING, "error, ret=%d, msg=%s\n", ret, errors);
        return result;
    }

    avformat_find_stream_info(inCtx, nullptr);
    for (int i = 0; i < inCtx->nb_streams; i++) {
        AVStream *inputStream = inCtx->streams[i];
        AVMediaType type = inputStream->codecpar->codec_type;
        if (!types.empty() && types.count(type) == 0) {
            continue;
        }

        result.insert({inCtx, inputStream});
    }
    return result;
}

int MediaMux::Open(const std::set<std::string> files, const std::string &outputFile)
{
    m_files = files;
    m_outputFile = outputFile.empty() ? "result.mp4" : outputFile;
    inputStreams = GetInputStreams(m_files);

    char errMsg[AV_ERROR_MAX_STRING_SIZE] = {'\0'};
    const int error = avformat_alloc_output_context2(&m_outCtx, nullptr, nullptr, m_outputFile.c_str());
    if (error < 0) {
        av_make_error_string(errMsg, AV_ERROR_MAX_STRING_SIZE, error);
        LOG_ERROR(LogGroup::MUX, "Failed to allocate output format context: {}", errMsg);
        return -2;
    }

    return 0;
}

void MediaMux::Close()
{
    if (m_outCtx) {
        const int ret = av_write_trailer(m_outCtx);
        if (ret != 0) {
            char errMsg[AV_ERROR_MAX_STRING_SIZE] = {'\0'};
            av_strerror(ret, errMsg, sizeof(errMsg));
            av_log(nullptr, AV_LOG_WARNING, "av_write_trailer error: ret=%d, msg=%s\n", ret, errMsg);
        }
        avio_close(m_outCtx->pb);
        m_outCtx = nullptr;
    }
}

int MediaMux::mux(bool autoCloseOutput)
{
    std::map<AVFormatContext *, std::map<int, int>> streamIndexMap;
    for (const auto &p : inputStreams) {
        AVFormatContext *inCtx = p.first;
        AVStream *inputStream = p.second;
        AVStream *newStream = avformat_new_stream(m_outCtx, nullptr);
        remux_copy_stream_info(inputStream, newStream);
        if (streamIndexMap[inCtx].count(inputStream->index)) {
            av_log(nullptr, AV_LOG_ERROR, "error, 重复映射了同一个输入流\n");
        }
        streamIndexMap[inCtx][inputStream->index] = newStream->index;
    }

    avio_open(&m_outCtx->pb, m_outputFile.c_str(), AVIO_FLAG_WRITE);

    AVDictionary *muxOpts = nullptr;
    av_dict_set(&muxOpts, "movflags", "use_metadata_tags", 0);
    const int ret = avformat_write_header(m_outCtx, &muxOpts);
    av_dict_free(&muxOpts);
    if (ret != 0) {
        return -2;
    }

    std::vector<AVFormatContext *> ctsx;
    for (auto it = inputStreams.begin(); it != inputStreams.end(); it = inputStreams.upper_bound(it->first)) {
        AVFormatContext *inCtx = it->first;
        LOG_INFO(LogGroup::MUX, "inCtx = {}", static_cast<const void *>(inCtx));
        ctsx.push_back(inCtx);
    }

    std::vector<ReadNode> readTasks(ctsx.size());
    {
        using namespace oneapi::tbb::flow;

        graph g;
        using PktsPtr = PacketBatchQueue *;

        broadcast_node<PktsPtr> start(g);
        std::vector<std::unique_ptr<function_node<PktsPtr, int>>> readNodes;

        PacketBatchQueue pktsRead;
        std::atomic<int> counter = 0;

        function_node<int, continue_msg> counter_node(g, serial, [&](int) -> continue_msg {
            counter.fetch_add(1);
            if (counter.load() >= static_cast<int>(readNodes.size())) {
                PacketsBatch eosBatch;
                eosBatch.m_pkts.push_back(g_EOSPacket);
                pktsRead.push(eosBatch);
            }
            return continue_msg{};
        });

        for (int i = 0; i < static_cast<int>(ctsx.size()); ++i) {
            auto readNode = std::make_unique<function_node<PktsPtr, int>>(g, unlimited, [&, i](PktsPtr pktsPtr) {
                PacketBatchQueue &pkts = *pktsPtr;
                return readTasks[i].ReadPackets(ctsx[i], streamIndexMap, pkts);
            });

            make_edge(start, *readNode);
            make_edge(*readNode, counter_node);
            readNodes.push_back(std::move(readNode));
        }

        function_node<PktsPtr> deal(g, unlimited, [&](PktsPtr pktsPtr) {
            PacketBatchQueue &pkts = *pktsPtr;
            DealPkts(m_outCtx, pkts);
        });
        make_edge(start, deal);

        pktsRead.set_capacity(10);
        start.try_put(&pktsRead);
        g.wait_for_all();
    }

    if (autoCloseOutput) {
        Close();
    }
    for (AVFormatContext *ctx : ctsx) {
        avformat_close_input(&ctx);
    }

    LOG_INFO(LogGroup::MUX, "All done!");
    return 0;
}
