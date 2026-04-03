#include "attach.h"

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
#include "Logger.h"

int AttachInfo::AttachMain()
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

    {
        auto folders = Tools::CollectBiliFolders(R"(C:\Users\Administrator\Desktop\bili_zip_1\)");
        for (auto &p : folders) {
            std::cout << p.string() << "\n";
        }
        std::cout << "Found " << folders.size() << " matching folders.\n";
        return 0;
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
bool AttachInfo::write_string_to_mp4_metadata(const std::wstring &in_filename,
                                            const std::wstring &out_filename,
                                            const std::string &keyName,
                                            const std::string &value)
{
    std::string in_utf8 = Tools::wstring_to_utf8(in_filename);
    std::string out_utf8 = Tools::wstring_to_utf8(out_filename);

    //av_log_set_level(AV_LOG_QUIET);

    AVFormatContext *ifmt_ctx = nullptr;
    AVFormatContext *ofmt_ctx = nullptr;
    AVDictionary *opts = nullptr;
    int ret = 0;

    // 1. open input
    if ((ret = avformat_open_input(&ifmt_ctx, in_utf8.c_str(), nullptr, nullptr)) < 0) {
        print_av_error(ret);
        return false;
    }
    if ((ret = avformat_find_stream_info(ifmt_ctx, nullptr)) < 0) {
        print_av_error(ret);
        avformat_close_input(&ifmt_ctx);
        return false;
    }

    // 2. allocate output context (auto-detect format by filename)
    if ((ret = avformat_alloc_output_context2(&ofmt_ctx, nullptr, nullptr, out_utf8.c_str())) < 0 || !ofmt_ctx) {
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
        if ((ret = avio_open(&ofmt_ctx->pb, out_utf8.c_str(), AVIO_FLAG_WRITE)) < 0) {
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


std::vector<AttachedFile> AttachInfo::ReadAttachments(AVFormatContext *ctx)
{
    std::vector<AttachedFile> attachments;

    if (!ctx) {
        LOG_ERROR("ffmpeg", "AVFormatContext is null");
        return attachments;
    }

    for (unsigned int i = 0; i < ctx->nb_streams; ++i) {
        AVStream *st = ctx->streams[i];
        if (!st)
            continue;

        if (st->codecpar->codec_type != AVMEDIA_TYPE_ATTACHMENT)
            continue;

        // 读取文件名
        AVDictionaryEntry *entry = av_dict_get(st->metadata, "filename", nullptr, 0);
        std::string fileName = entry ? entry->value : "unknown";

        // 读取内容
        std::string content;
        if (st->codecpar->extradata && st->codecpar->extradata_size > 0) {
            content.assign((char *)st->codecpar->extradata, st->codecpar->extradata_size);
        }
        else {
            LOG_WARN("ffmpeg", "Attachment stream {} has no extradata", fileName);
        }

        LOG_INFO("ffmpeg", "Read attachment: {} ({} bytes)", fileName, content.size());

        attachments.push_back({fileName, content});
    }

    return attachments;
}

AVStream *AttachInfo::MakeAttachStream(AVFormatContext *ctx, std::wstring fileName)
{
    AVStream *st = avformat_new_stream(ctx, nullptr);
    st->codecpar->codec_type = AVMEDIA_TYPE_ATTACHMENT;
    st->codecpar->codec_id = AV_CODEC_ID_NONE;
    // 文件名
    std::string utf8Name = Tools::wstring_to_utf8(fileName);
    av_dict_set(&st->metadata, "filename", utf8Name.c_str(), 0);
    return st;
}

std::string AttachInfo::GetAttachStreamName(AVStream *st)
{
    AVDictionaryEntry *entry = av_dict_get(st->metadata, "filename", nullptr, 0);
    if (entry == nullptr) {
        return false;
    }
    std::string name = entry->value ? entry->value : "";
    return name;
}

bool AttachInfo::IsAttachStream(AVStream *st)
{
    std::string fileName;
    if (st->codecpar->codec_type != AVMEDIA_TYPE_ATTACHMENT) {
        return false;
    }
    if (st->codecpar->codec_id != AV_CODEC_ID_NONE) {
        return false;
    }
    // 读取文件名
    AVDictionaryEntry *entry = av_dict_get(st->metadata, "filename", nullptr, 0);
    if (entry == nullptr) {
        return false;
    }

    fileName = entry->value;
    return false;
}

std::multimap<std::string, AVStream *> AttachInfo::GetAllAttachStream(AVFormatContext *ctx)
{
    std::multimap<std::string, AVStream *> ret;
    for (size_t i = 0; i < ctx->nb_streams; i++) {
        AVStream *st = ctx->streams[i];
        if (IsAttachStream(st) == false) {
            continue;
        }
        std::string name = GetAttachStreamName(st);
        ret.insert({name, st});
    }

    return ret;
}

bool AttachInfo::write_attach(AVFormatContext *ctx, const MediaSubdir &media)
{
    std::vector<std::wstring> files{
        media.match->danmaku_xml_path.generic_wstring(),
        media.match->entry_json_path.generic_wstring(),
        media.index.generic_wstring(),
    };

    std::multimap<std::string, AVStream *> attStms = GetAllAttachStream(ctx);

    for (size_t i = 0; i < files.size(); i++) {
        std::wstring file = files[i];
        //std::string fileU8 = Tools::wstring_to_utf8(file);
        std::string txt;
        Tools::StringFromFile(file, txt);
        std::string fileNameU8 = fs::path(file).filename().generic_u8string();
        std::wstring fileNameW = fs::path(file).filename().generic_wstring();

        // 文件内容
        AVStream *st = nullptr;
        int count = attStms.count(fileNameU8);
        if(count > 0) {
            LOG_WARN(LogGroup::DEFAULT, "Attachment {} already exists {} , using first one",
                  fileNameU8, count);
            st = attStms.find(fileNameU8)->second;
        } else {
            st = MakeAttachStream(ctx, fileNameW);
        }
        st->codecpar->extradata_size = txt.size();
        st->codecpar->extradata = (uint8_t *)av_malloc(txt.size());
        memcpy(st->codecpar->extradata, txt.data(), txt.size());

        LOG_DEBUG(LogGroup::DEFAULT, "---{} -- {}", fileNameU8, txt[0]);
    }

    if (ctx->metadata == nullptr) {
        return false;
    }
    return false;
}