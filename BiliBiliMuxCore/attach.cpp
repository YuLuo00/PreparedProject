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

#include "tools.h"

int AttachMain()
{
    std::vector<std::string> files = {
        //R"(C:\Users\Administrator\Desktop\bili_zip_1\10723293\1\danmaku.xml)",
        //R"(C:\Users\Administrator\Desktop\bili_zip_1\10723293\1\80\index.json)",
        R"(C:\Users\Administrator\Desktop\bili_zip_1\10723293\1\entry.json)",
    };

    for (size_t i = 0; i < files.size(); i++) {
        std::string txt;
        std::string file = files[i];
        Tools::TextFromFile(files[i], txt);
        nlohmann::json js;
        Tools::ParseJsonSafe(txt, js);
        std::string pretty = js.dump(2); // indent = 2 空格缩进
        std::string title = js.at("title").get<std::string>();
        continue;
    }

    return 1;
}

// compile with: g++ -std=c++11 write_metadata.cpp -o write_metadata `pkg-config --cflags --libs libavformat libavcodec libavutil`
#include <iostream>
#include <memory>
#include <string>

extern "C"
{
#include <libavformat/avformat.h>
#include <libavutil/dict.h>
#include <libavutil/error.h>
}

static void print_av_error(int err)
{
    char buf[256];
    av_strerror(err, buf, sizeof(buf));
    std::cerr << "FFmpeg error: " << buf << "\n";
}

// keyName should be a short ASCII key, e.g. "my_blob"
bool write_string_to_mp4_metadata(const std::string &in_filename,
                                  const std::string &out_filename,
                                  const std::string &keyName,
                                  const std::string &value)
{
    //av_log_set_level(AV_LOG_QUIET);

    AVFormatContext *ifmt_ctx = nullptr;
    AVFormatContext *ofmt_ctx = nullptr;
    AVDictionary *opts = nullptr;
    int ret = 0;

    // 1. open input
    if ((ret = avformat_open_input(&ifmt_ctx, in_filename.c_str(), nullptr, nullptr)) < 0) {
        print_av_error(ret);
        return false;
    }
    if ((ret = avformat_find_stream_info(ifmt_ctx, nullptr)) < 0) {
        print_av_error(ret);
        avformat_close_input(&ifmt_ctx);
        return false;
    }

    // 2. allocate output context (auto-detect format by filename)
    if ((ret = avformat_alloc_output_context2(&ofmt_ctx, nullptr, nullptr, out_filename.c_str())) < 0 || !ofmt_ctx) {
        print_av_error(ret);
        avformat_close_input(&ifmt_ctx);
        return false;
    }

    // 3. copy streams (remux)
    for (unsigned i = 0; i < ifmt_ctx->nb_streams; ++i) {
        AVStream *in_stream = ifmt_ctx->streams[i];
        AVStream *out_stream = avformat_new_stream(ofmt_ctx, nullptr);
        if (!out_stream) {
            std::cerr << "Failed to create output stream\n";
            ret = AVERROR_UNKNOWN;
            goto end;
        }
        ret = avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar);
        if (ret < 0) {
            print_av_error(ret);
            goto end;
        }
        out_stream->codecpar->codec_tag = 0;
        // keep same time_base for rescaling later
        out_stream->time_base = in_stream->time_base;
    }

    // 4. set metadata on output context
    // Note: av_dict_set will copy the value; keyName must be ASCII and reasonably short
    av_dict_set(&ofmt_ctx->metadata, keyName.c_str(), value.c_str(), 0);

    // 5. open output IO
    if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        if ((ret = avio_open(&ofmt_ctx->pb, out_filename.c_str(), AVIO_FLAG_WRITE)) < 0) {
            print_av_error(ret);
            goto end;
        }
    }

    // 6. write header
    if ((ret = avformat_write_header(ofmt_ctx, &opts)) < 0) {
        print_av_error(ret);
        goto end;
    }

    // 7. read packets from input and write to output (rescale timestamps)
    AVPacket pkt;
    av_init_packet(&pkt);
    while ((ret = av_read_frame(ifmt_ctx, &pkt)) >= 0) {
        AVStream *in_stream = ifmt_ctx->streams[pkt.stream_index];
        AVStream *out_stream = ofmt_ctx->streams[pkt.stream_index];

        // rescale timestamps
        pkt.pts = av_rescale_q_rnd(pkt.pts,
                                   in_stream->time_base,
                                   out_stream->time_base,
                                   (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        pkt.dts = av_rescale_q_rnd(pkt.dts,
                                   in_stream->time_base,
                                   out_stream->time_base,
                                   (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
        pkt.pos = -1;
        pkt.stream_index = pkt.stream_index;

        if ((ret = av_interleaved_write_frame(ofmt_ctx, &pkt)) < 0) {
            print_av_error(ret);
            av_packet_unref(&pkt);
            goto end;
        }
        av_packet_unref(&pkt);
    }
    if (ret == AVERROR_EOF)
        ret = 0;

    // 8. write trailer
    if ((ret = av_write_trailer(ofmt_ctx)) < 0) {
        print_av_error(ret);
        goto end;
    }

end:
    if (ifmt_ctx)
        avformat_close_input(&ifmt_ctx);
    if (ofmt_ctx) {
        if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE))
            avio_closep(&ofmt_ctx->pb);
        avformat_free_context(ofmt_ctx);
    }
    if (ret < 0)
        return false;
    return true;
}

// Example usage
int main1(int argc, char **argv)
{
    if (argc < 5) {
        std::cerr << "Usage: " << argv[0] << " input.mp4 output.mp4 keyName value\n";
        return 1;
    }
    std::string in = argv[1];
    std::string out = argv[2];
    std::string key = argv[3];
    std::string value = argv[4];

    avformat_network_init();
    bool ok = write_string_to_mp4_metadata(in, out, key, value);
    avformat_network_deinit();

    if (ok) {
        std::cout << "Wrote metadata key '" << key << "' to " << out << "\n";
        return 0;
    }
    else {
        std::cerr << "Failed to write metadata\n";
        return 2;
    }
}
