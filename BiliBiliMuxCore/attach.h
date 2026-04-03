#pragma once

#include <string>

#include "common.h"
#include "BiliCache.h"

namespace AttachNs
{

int AttachMain();
bool write_string_to_mp4_metadata(const std::string &in_filename,
                                  const std::string &out_filename,
                                  const std::string &keyName,
                                  const std::string &value);
bool write_attach(AVFormatContext *ctx, const MediaSubdir &meia);
}
