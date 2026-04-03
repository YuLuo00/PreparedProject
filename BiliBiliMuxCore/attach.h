#pragma once

#include <string>
#include <vector>

#include "common.h"
#include "BiliCache.h"

struct AttachedFile
{
    std::string fileName;
    std::string content;
};


class AttachInfo
{
public:
    static int AttachMain();
    static bool write_string_to_mp4_metadata(const std::string &in_filename,
                                             const std::string &out_filename,
                                             const std::string &keyName,
                                             const std::string &value);
    static bool write_attach(AVFormatContext *ctx, const MediaSubdir &meia);
    static std::vector<AttachedFile> ReadAttachments(AVFormatContext *ctx);
    static AVStream *MakeAttachStream(AVFormatContext *ctx, std::string fileName);

    static std::string GetAttachStreamName(AVStream *st);

    static bool IsAttachStream(AVStream *st);

    static std::multimap<std::string, AVStream *> GetAllAttachStream(AVFormatContext *ctx);
};
