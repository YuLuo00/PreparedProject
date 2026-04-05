#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/avutil.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}

#include <string>

// Open an image file and provide the first video stream plus decoder context.
// This is used to read a cover image and pack it into AVStream.attached_pic.
int openImageFile(const char *file, AVFormatContext *&formatContext, AVCodecContext *&videoContext,
                  AVStream *&videoStream) {
    int ret = avformat_open_input(&formatContext, file, nullptr, nullptr);
    if (ret < 0) {
        return -1;
    }

    ret = avformat_find_stream_info(formatContext, nullptr);
    if (ret < 0) {
        return -1;
    }

    for (int i = 0; i < formatContext->nb_streams; ++i) {
        if (formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStream = formatContext->streams[i];
            const AVCodec *codec = avcodec_find_decoder(videoStream->codecpar->codec_id);
            videoContext = avcodec_alloc_context3(codec);
            avcodec_parameters_to_context(videoContext, videoStream->codecpar);
            avcodec_open2(videoContext, codec, nullptr);
            break;
        }
    }

    if (!videoStream) {
        return -1;
    }

    return 0;
}

// Open a media file and provide the first audio/video streams plus decoder contexts.
// This is used to read the source video and audio packets for remuxing.
int openVideoFile(const char *file, AVFormatContext *&formatContext, AVCodecContext *&audioContext,
                  AVCodecContext *&videoContext, AVStream *&audioStream, AVStream *&videoStream) {
    int ret = avformat_open_input(&formatContext, file, nullptr, nullptr);
    if (ret < 0) {
        return -1;
    }

    ret = avformat_find_stream_info(formatContext, nullptr);
    if (ret < 0) {
        return -1;
    }

    for (int i = 0; i < formatContext->nb_streams; ++i) {
        AVStream *stream = formatContext->streams[i];
        if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStream = stream;
            const AVCodec *codec = avcodec_find_decoder(videoStream->codecpar->codec_id);
            videoContext = avcodec_alloc_context3(codec);
            avcodec_parameters_to_context(videoContext, videoStream->codecpar);
            avcodec_open2(videoContext, codec, nullptr);
        } else if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audioStream = stream;
            const AVCodec *codec = avcodec_find_decoder(audioStream->codecpar->codec_id);
            audioContext = avcodec_alloc_context3(codec);
            avcodec_parameters_to_context(audioContext, audioStream->codecpar);
            avcodec_open2(audioContext, codec, nullptr);
        }

        if (videoStream && audioStream) {
            break;
        }
    }

    if (!videoStream || !audioContext) {
        return -1;
    }

    return 0;
}

// Add a cover image to an existing video file.
// The resulting file contains: video stream, audio stream, and an attached picture stream.
int add_cover_to_video(const char *output_filename, const char *input_filename, const char *image_filename) {
    int ret = 0;

    AVFormatContext *inFmtContext = nullptr;
    AVFormatContext *imageFmtContext = nullptr;
    AVFormatContext *outFmtContext = nullptr;
    AVCodecContext *inAudioContext = nullptr;
    AVCodecContext *inVideoContext = nullptr;
    AVCodecContext *imageVideoContext = nullptr;

    AVStream *inAudioStream = nullptr;
    AVStream *inVideoStream = nullptr;
    AVStream *outAudioStream = nullptr;
    AVStream *outVideoStream = nullptr;
    AVStream *imageVideoStream = nullptr;

    // Open source video and image files.
    ret = openVideoFile(input_filename, inFmtContext, inAudioContext, inVideoContext, inAudioStream, inVideoStream);
    if (ret < 0) {
        return ret;
    }

    ret = openImageFile(image_filename, imageFmtContext, imageVideoContext, imageVideoStream);
    if (ret < 0) {
        return ret;
    }

    // Create output format context for the target file.
    ret = avformat_alloc_output_context2(&outFmtContext, nullptr, nullptr, output_filename);
    if (ret < 0 || !outFmtContext) {
        return -1;
    }

    // Create the output video stream by copying codec parameters from input video.
    outVideoStream = avformat_new_stream(outFmtContext, nullptr);
    if (!outVideoStream) {
        return -1;
    }
    ret = avcodec_parameters_copy(outVideoStream->codecpar, inVideoStream->codecpar);
    if (ret < 0) {
        return -1;
    }
    outVideoStream->codecpar->codec_tag = 0;

    // Create the output audio stream from the input audio decoder context.
    const AVCodec *audioCodec = avcodec_find_encoder(inAudioStream->codecpar->codec_id);
    outAudioStream = avformat_new_stream(outFmtContext, audioCodec);
    if (!outAudioStream) {
        return -1;
    }
    ret = avcodec_parameters_from_context(outAudioStream->codecpar, inAudioContext);
    if (ret < 0) {
        return -1;
    }

    // Create the attached picture stream for the cover.
    AVStream *coverStream = avformat_new_stream(outFmtContext, nullptr);
    if (!coverStream) {
        return -1;
    }
    ret = avcodec_parameters_copy(coverStream->codecpar, imageVideoStream->codecpar);
    if (ret < 0) {
        return -1;
    }
    coverStream->disposition = AV_DISPOSITION_ATTACHED_PIC;

    // Read one packet from the image input and assign it to attached_pic.
    ret = av_read_frame(imageFmtContext, &coverStream->attached_pic);
    if (ret < 0) {
        return -1;
    }
    coverStream->attached_pic.stream_index = coverStream->index;
    coverStream->attached_pic.flags |= AV_PKT_FLAG_KEY;

    // Copy metadata from source files to output streams.
    av_dict_copy(&outFmtContext->metadata, inFmtContext->metadata, 0);
    av_dict_copy(&outVideoStream->metadata, inVideoStream->metadata, 0);
    av_dict_copy(&outAudioStream->metadata, inAudioStream->metadata, 0);
    av_dict_copy(&coverStream->metadata, imageVideoStream->metadata, 0);

    if (!(outFmtContext->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&outFmtContext->pb, output_filename, AVIO_FLAG_WRITE);
        if (ret < 0) {
            return -1;
        }
    }

    // Write container header before writing any packets.
    ret = avformat_write_header(outFmtContext, nullptr);
    if (ret < 0) {
        return -1;
    }

    // Write the attached picture packet explicitly.
    ret = av_interleaved_write_frame(outFmtContext, &coverStream->attached_pic);
    if (ret < 0) {
        return -1;
    }

    // Read packets from the source video and audio streams and remap them into the output.
    while (true) {
        AVPacket packet;
        av_init_packet(&packet);
        packet.data = nullptr;
        packet.size = 0;

        ret = av_read_frame(inFmtContext, &packet);
        if (ret == AVERROR_EOF) {
            av_packet_unref(&packet);
            break;
        }
        if (ret < 0) {
            av_packet_unref(&packet);
            break;
        }

        if (packet.flags & AV_PKT_FLAG_DISCARD) {
            av_packet_unref(&packet);
            continue;
        }

        if (packet.stream_index == inVideoStream->index) {
            packet.stream_index = outVideoStream->index;
            av_packet_rescale_ts(&packet, inVideoStream->time_base, outVideoStream->time_base);
            packet.duration = av_rescale_q(packet.duration, inVideoStream->time_base, outVideoStream->time_base);
            packet.pos = -1;
            av_interleaved_write_frame(outFmtContext, &packet);
        } else if (packet.stream_index == inAudioStream->index) {
            packet.stream_index = outAudioStream->index;
            av_packet_rescale_ts(&packet, inAudioStream->time_base, outAudioStream->time_base);
            av_interleaved_write_frame(outFmtContext, &packet);
        }

        av_packet_unref(&packet);
    }

    av_write_trailer(outFmtContext);

    if (!(outFmtContext->oformat->flags & AVFMT_NOFILE)) {
        avio_closep(&outFmtContext->pb);
    }

    // Release all allocated resources.
    avformat_free_context(outFmtContext);
    avformat_free_context(inFmtContext);
    avformat_free_context(imageFmtContext);
    avcodec_free_context(&inAudioContext);
    avcodec_free_context(&inVideoContext);
    avcodec_free_context(&imageVideoContext);

    return 0;
}

int main34(int argc, char* argv[]) {
    if (argc != 4) {
        return -1;
    }

    std::string video_input = argv[1];
    std::string image_input = argv[2];
    std::string video_output = argv[3];

    return add_cover_to_video(video_output.c_str(), video_input.c_str(), image_input.c_str());
}
