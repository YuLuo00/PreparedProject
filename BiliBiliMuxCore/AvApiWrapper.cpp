#include "AvApiWrapper.h"

#include "Logger.h"

AVFormatContext *AvApiWrapper::_AvformatAllocOutputContext2(const std::wstring &file, std::string *err)
{
    std::string fileU8 = Tools::wstring_to_utf8(file);
    AVFormatContext *outCtx = nullptr;
    char errMsg[AV_ERROR_MAX_STRING_SIZE] = {'\0'};
    int error = avformat_alloc_output_context2(&outCtx, nullptr, nullptr, fileU8.c_str());
    if (error < 0) {
        // 输出错误代码及错误信息
        av_make_error_string(errMsg, AV_ERROR_MAX_STRING_SIZE, error);
        *err = errMsg;
        LOG_ERROR(LogGroup::IO, "Failed to allocate output format context: {}", errMsg);
        // 处理错误情况
    }
    return outCtx;
}
