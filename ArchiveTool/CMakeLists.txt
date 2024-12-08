cmake_minimum_required(VERSION 3.18)

include("${CMAKE_CURRENT_LIST_DIR}/../ProjectRefrence.cmake")

project(ArchiveTool)

# 定义库
add_library(${PROJECT_NAME} SHARED)
# add_executable(${PROJECT_NAME})
set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${ProjectRootDir}/bin"
    VS_DEBUGGER_COMMAND "${ProjectRootDir}/bin/${PROJECT_NAME}.exe"
)

# 三方库
Add3rd_bit7z(${PROJECT_NAME})
Add3rd_LibArchive(${PROJECT_NAME})
Add3rd_nlohmann_json(${PROJECT_NAME})
Add3rd_fmt(${PROJECT_NAME})

# 包含目录
target_include_directories(${PROJECT_NAME} PRIVATE
    "${ProjectRootDir}/ArchiveTool"
    "${ProjectRootDir}/ArchiveTool/src"
    "${ProjectRootDir}/ArchiveTool/interface"
)

# 源文件
target_sources_group(${PROJECT_NAME} "src" PRIVATE
    "${ProjectRootDir}/ArchiveTool/src/CommonTool.cpp"
    "${ProjectRootDir}/ArchiveTool/src/CommonTool.h"
    "${ProjectRootDir}/ArchiveTool/src/Global.h"
    "${ProjectRootDir}/ArchiveTool/src/Global.cpp"
    "${ProjectRootDir}/ArchiveTool/src/ArchiveType.h"
    "${ProjectRootDir}/ArchiveTool/src/ArchiveType.cpp"
    "${ProjectRootDir}/ArchiveTool/src/ArchiveMsg.h"
    "${ProjectRootDir}/ArchiveTool/src/ArchiveMsg.cpp"
    "${ProjectRootDir}/ArchiveTool/src/PwdManager.cpp"
    "${ProjectRootDir}/ArchiveTool/src/ArchiveToolMain.cpp"
    "${ProjectRootDir}/ArchiveTool/src/ArchiveTool.cpp"
)
target_sources_group(${PROJECT_NAME} "interface" PRIVATE
    "${ProjectRootDir}/ArchiveTool/interface/PwdManager.h"
    "${ProjectRootDir}/ArchiveTool/interface/ArchiveTool.h"
    
)

# dll导出宏
target_compile_definitions(${PROJECT_NAME} PRIVATE ZYB_ARCHIVE_TOOL_API=__declspec\(dllexport\))
target_compile_definitions(${PROJECT_NAME} INTERFACE ZYB_ARCHIVE_TOOL_API=__declspec\(dllimport\))
target_include_directories(${PROJECT_NAME} INTERFACE "${ProjectRootDir}/ArchiveTool/interface/")

# 安装
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION bin
    ARCHIVE DESTINATION lib
)
install(DIRECTORY
        "${ProjectRootDir}/ArchiveTool/interface/"
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)
install(FILES
        # ${ALL_IMPORTED_LOCATION}
        $<$<CONFIG:Debug>:${ALL_IMPORTED_LOCATION_Debug}>
        $<$<CONFIG:Release>:${ALL_IMPORTED_LOCATION_Release}>
    DESTINATION bin
)