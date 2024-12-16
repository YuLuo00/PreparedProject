#include <iostream>
#include <string>
#include <string.h>

#include "ArchiveTool.h"

void main()
{
    if (ArchiveExtraTest(L"bin.rar", L"test", L"")) {
        exit(0);
    }

    exit(-1);

}
