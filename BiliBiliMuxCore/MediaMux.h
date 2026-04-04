#pragma once

#include <map>
#include <set>
#include <string>
#include <filesystem>
namespace fs = std::filesystem;

#include "common.h"


class MediaMux
{
public:
    MediaMux() = default;
    ~MediaMux()
    {
        Close();
    }

    static std::multimap<AVFormatContext *, AVStream *> GetInputStreams(const std::set<std::string> &files,
                                                                        std::set<AVMediaType> types = {});
    static std::multimap<AVFormatContext *, AVStream *> GetInputStreams(const std::string &file,
                                                                        std::set<AVMediaType> types = {});

    int Open(const std::set<std::string> files, const std::string &outputFile = "result.mp4");
    void Close();
    int mux(bool autoCloseOutput = true);
    int EmbedCover(const std::filesystem::path &coverPath);

    AVFormatContext *m_outCtx = nullptr;
    std::set<std::string> m_files;
    std::string m_outputFile = "result.mp4";
    std::multimap<AVFormatContext *, AVStream *> inputStreams;
};
