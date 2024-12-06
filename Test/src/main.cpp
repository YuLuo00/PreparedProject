#include <iostream>
#include <string>
#include <string.h>

#include "ArchiveTool.h"


void main()
{
    if (ArchiveExtraTest("bin.rar", "test")) {
        exit(0);
    }

    exit(-1);

}
