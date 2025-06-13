#ifndef RAW_MOVE_MAIN_H
#define RAW_MOVE_MAIN_H

#include <string>
#include <functional>
#include <set>

#include "Msg2UI.h"

ZYB_RAWMOVE_API void say_hello();


class ZYB_RAWMOVE_API RawMoveCore
{
public:
    RawMoveCore() = default;
    ~RawMoveCore() = default;

    int GetNum();

    Msg2UI m_msg2ui;

    std::wstring m_rootDirPath;
    std::wstring m_RawsStorePath;
    bool MoveAllRawToStorePath();
};

#endif // !RAW_MOVE_MAIN_H



