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

#include <string>

//打开图片文件,获取流信息
//@1文件地址  @2媒体上下文  @3视频解码器上下文  @4视频流
int openImageFile(const char *file, AVFormatContext *&formatContext, AVCodecContext *&videoContext,
	AVStream *&videoStream) {
	int ret = 0;
	ret = avformat_open_input(&formatContext, file, nullptr, nullptr);
	if (ret < 0) 
	{
		return -1;
	}
	ret = avformat_find_stream_info(formatContext, nullptr);
	if (ret < 0) 
	{
		return -1;
	}

	for (int j = 0; j < formatContext->nb_streams; ++j) {
		if (formatContext->streams[j]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
			videoStream = formatContext->streams[j];
            const AVCodec *codec = avcodec_find_decoder(videoStream->codecpar->codec_id);
			videoContext = avcodec_alloc_context3(codec);
			avcodec_parameters_to_context(videoContext, videoStream->codecpar);
			avcodec_open2(videoContext, codec, nullptr);
		}
	}
	if (!videoStream)
	{
		return -1;
	}
	return 0;
}

//打开视频文件，获取流信息
//@1文件地址 @2媒体上下文  @3音频解码器上下文  @4视频解码器上下文  @5音频流 
//@6视频流
int openVideoFile(const char *file, AVFormatContext *&formatContext, AVCodecContext *&audioContext,
	AVCodecContext *&videoContext, AVStream *&audioStream, AVStream *&videoStream) {
	int ret = 0;
	ret = avformat_open_input(&formatContext, file, nullptr, nullptr);
	if (ret < 0) 
	{
		return -1;
	}
	ret = avformat_find_stream_info(formatContext, nullptr);
	if (ret < 0) 
	{
		return -1;
	}

	for (int j = 0; j < formatContext->nb_streams; ++j) {
		if (formatContext->streams[j]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
			videoStream = formatContext->streams[j];
			const AVCodec *codec = avcodec_find_decoder(videoStream->codecpar->codec_id);
			videoContext = avcodec_alloc_context3(codec);
			avcodec_parameters_to_context(videoContext, videoStream->codecpar);
			avcodec_open2(videoContext, codec, nullptr);
		}
		else if (formatContext->streams[j]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
			audioStream = formatContext->streams[j];
            const AVCodec *codec = avcodec_find_decoder(audioStream->codecpar->codec_id);
			audioContext = avcodec_alloc_context3(codec);
			avcodec_parameters_to_context(audioContext, audioStream->codecpar);
			avcodec_open2(audioContext, codec, nullptr);
		}
		if (videoStream && audioStream) break;
	}

	if (!videoStream) 
	{
		return -1;
	}
	if (!audioContext) 
	{
		return -1;
	}

	return 0;
}

//给视频添加自定义封面
//@1输出视频地址  @2输入视频地址 @3图片地址
int add_cover_to_video(const char *output_filename, const char *input_filename, const char *image_filename)
{
	int ret = 0;
	//各种解码器的上下文
	AVFormatContext *outFmtContext = nullptr;
	AVFormatContext *inFmtContext = nullptr;
	AVFormatContext *imageFmtContext = nullptr;
	AVCodecContext *inAudioContext = nullptr;
	AVCodecContext *inVideoContext = nullptr;
	AVCodecContext *outAudioContext = nullptr;
	AVCodecContext *imageVideoContext = nullptr;

	//音视频流信息
	AVStream *inAudioStream = nullptr;
	AVStream *inVideoStream = nullptr;
	AVStream *outAudioStream = nullptr;
	AVStream *outVideoStream = nullptr;
	AVStream *imageVideoStream = nullptr;

    const AVCodec *audioCodec = nullptr;

	//打开视频文件获取上下文
	ret = openVideoFile(input_filename, inFmtContext, inAudioContext, inVideoContext, inAudioStream,
		inVideoStream);
	if (ret < 0) return ret;

	//打开图片文件获取上下文
	ret = openImageFile(image_filename, imageFmtContext, imageVideoContext, imageVideoStream);
	if (ret < 0) return ret;

	//创建输出的上下文
	ret = avformat_alloc_output_context2(&outFmtContext, nullptr, nullptr, output_filename);


	//输出视频流
	outVideoStream = avformat_new_stream(outFmtContext, nullptr);
	if (!outVideoStream) 
	{
		return -1;
	}
	outVideoStream->id = outFmtContext->nb_streams - 1;
	ret = avcodec_parameters_copy(outVideoStream->codecpar, inVideoStream->codecpar);
	if (ret < 0) 
	{
		return -1;
	}
	outVideoStream->codecpar->codec_tag = 0;

	//输出音频流
	audioCodec = avcodec_find_encoder(inAudioStream->codecpar->codec_id);
	outAudioStream = avformat_new_stream(outFmtContext, audioCodec);
	if (!outAudioStream) 
	{
		return -1;
	}
	ret = avcodec_parameters_from_context(outAudioStream->codecpar, inAudioContext);
	if (ret < 0)
	{
		return -1;
	}

	//视频封面对应的流
	AVStream* cover_stream = avformat_new_stream(outFmtContext, nullptr);
	outVideoStream->id = outFmtContext->nb_streams - 1;
	ret = avcodec_parameters_copy(cover_stream->codecpar, imageVideoStream->codecpar);
	cover_stream->disposition = AV_DISPOSITION_ATTACHED_PIC;
	av_read_frame(imageFmtContext, &cover_stream->attached_pic);
	cover_stream->attached_pic.stream_index = outFmtContext->nb_streams - 1;
	cover_stream->attached_pic.flags |= AV_PKT_FLAG_KEY;


	//拷贝原始数据
	av_dict_copy(&outFmtContext->metadata, inFmtContext->metadata, 0);
	av_dict_copy(&outVideoStream->metadata, inVideoStream->metadata, 0);
	av_dict_copy(&outAudioStream->metadata, inAudioStream->metadata, 0);
	av_dict_copy(&cover_stream->metadata, imageVideoStream->metadata, 0);

	if (!(outFmtContext->oformat->flags & AVFMT_NOFILE)) 
	{
		ret = avio_open(&outFmtContext->pb, output_filename, AVIO_FLAG_WRITE);
		if (ret < 0) 
		{
			return -1;
		}
	}


	//写文件头
	ret = avformat_write_header(outFmtContext, nullptr);
	if (ret < 0)
	{
		return -1;
	}

	//写封面对应的数据包
	ret = av_interleaved_write_frame(outFmtContext, &cover_stream->attached_pic);
	AVFrame *inputFrame = av_frame_alloc();
	do 
	{
		AVPacket packet{ nullptr };
		av_init_packet(&packet);

		ret = av_read_frame(inFmtContext, &packet);
		if (ret == AVERROR_EOF)
		{
			break;
		}
		else if (ret < 0)
		{
			break;
		}

		if (packet.flags & AV_PKT_FLAG_DISCARD) continue;
		//写视频流和音频流
		if (packet.stream_index == inVideoStream->index) 
		{
			packet.stream_index = outVideoStream->index;
			av_packet_rescale_ts(&packet, inVideoStream->time_base, outVideoStream->time_base);
			packet.duration = av_rescale_q(packet.duration, inVideoStream->time_base, outVideoStream->time_base);
			packet.pos = -1;
			ret = av_interleaved_write_frame(outFmtContext, &packet);
		}
		else if (packet.stream_index == inAudioStream->index)
		{
			packet.stream_index = outAudioStream->index;
			av_packet_rescale_ts(&packet, inAudioStream->time_base, outAudioStream->time_base);
			ret = av_interleaved_write_frame(outFmtContext, &packet);
		}

	} while (true);

	av_write_trailer(outFmtContext);

	if (!(outFmtContext->oformat->flags & AVFMT_NOFILE)) 
	{
		avio_closep(&outFmtContext->pb);
	}

	//清理分配之后的数据
	av_frame_free(&inputFrame);
	avformat_free_context(outFmtContext);
	avformat_free_context(inFmtContext);
	avformat_free_context(imageFmtContext);
	avcodec_free_context(&inAudioContext);
	avcodec_free_context(&inVideoContext);
	avcodec_free_context(&imageVideoContext);

	return 0;
}

int main34(int argc, char* argv[])
{
	if (argc != 4)
	{
		return -1;
	}
	std::string video_input = std::string(argv[1]);    //视频输入地址
	std::string image_input = std::string(argv[2]);    //图片地址
	std::string video_output = std::string(argv[3]);   //视频输出

	add_cover_to_video(video_output.c_str(), video_input.c_str(), image_input.c_str());
}