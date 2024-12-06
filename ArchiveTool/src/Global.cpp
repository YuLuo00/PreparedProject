#include "Global.h"

#include <iostream>
#include <windows.h>

#include <bit7z/bitfileextractor.hpp>


bit7z::Bit7zLibrary &Get7zLibrary()
{
    static bit7z::Bit7zLibrary lib{ "7zip.dll" };
    return lib;
}