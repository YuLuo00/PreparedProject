#include "Global.h"

#include <iostream>
#include <windows.h>

#include <bit7z/bitfileextractor.hpp>


bit7z::Bit7zLibrary &Get7zLibrary()
{
// #ifdef __MINGW32__
    // static bit7z::Bit7zLibrary lib{ std::wstring( L"lib7zip.dll" ) };
// #else
    // static bit7z::Bit7zLibrary lib{ std::wstring( L"7zip.dll" ) };
    static bit7z::Bit7zLibrary lib{ std::wstring( L"7z.dll" ) };
    // static bit7z::Bit7zLibrary lib{ std::wstring( L"7zxa.dll" ) };
// #endif
    return lib;
}
