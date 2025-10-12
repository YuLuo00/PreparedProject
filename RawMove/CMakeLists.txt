cmake_minimum_required(VERSION 3.18)

include("${CMAKE_CURRENT_LIST_DIR}/../ProjectRefrence.cmake")

project(AutoWinUI)

# 定义库
# add_library(${PROJECT_NAME} SHARED)
add_executable(${PROJECT_NAME})
set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${ProjectRootDir}/bin"
    VS_DEBUGGER_COMMAND "${ProjectRootDir}/bin/${PROJECT_NAME}.exe"
)

# 三方库
Add3rd_fmt(${PROJECT_NAME})
Add3rd_sqlite3(${PROJECT_NAME})
Add3rd_OpenCV(${PROJECT_NAME})

# 预定义宏
target_compile_definitions(${PROJECT_NAME} PRIVATE NOMINMAX)


# 包含目录
target_include_directories(${PROJECT_NAME} PRIVATE
    "${ProjectRootDir}/${CURRENT_FOLDER_NAME}"
    "${ProjectRootDir}/${CURRENT_FOLDER_NAME}/src"
    "${ProjectRootDir}/${CURRENT_FOLDER_NAME}/interface"
)

# 源文件
target_sources_group(${PROJECT_NAME} "interface" PRIVATE
    "${ProjectRootDir}/${CURRENT_FOLDER_NAME}/interface/RawMoveMain.h"
    "${ProjectRootDir}/${CURRENT_FOLDER_NAME}/interface/Msg2UI.h"
)
target_sources_group(${PROJECT_NAME} "src" PRIVATE
    "${ProjectRootDir}/${CURRENT_FOLDER_NAME}/src/AutoWins.cpp"
    "${ProjectRootDir}/${CURRENT_FOLDER_NAME}/src/AutoWins.h"
    "${ProjectRootDir}/${CURRENT_FOLDER_NAME}/src/CommonTool.cpp"
    "${ProjectRootDir}/${CURRENT_FOLDER_NAME}/src/CommonTool.h"
    "${ProjectRootDir}/${CURRENT_FOLDER_NAME}/src/ImageTools.cpp"
    "${ProjectRootDir}/${CURRENT_FOLDER_NAME}/src/ImageTools.h"
    "${ProjectRootDir}/${CURRENT_FOLDER_NAME}/src/Msg2UI.cpp"
    "${ProjectRootDir}/${CURRENT_FOLDER_NAME}/src/RawMove.cpp"
    "${ProjectRootDir}/${CURRENT_FOLDER_NAME}/src/RawMove.h"
    "${ProjectRootDir}/${CURRENT_FOLDER_NAME}/src/RawMoveMain.cpp"
    "${ProjectRootDir}/${CURRENT_FOLDER_NAME}/src/WinTool.cpp"
    "${ProjectRootDir}/${CURRENT_FOLDER_NAME}/src/WinTool.h"
)

# dll导出宏
target_compile_definitions(${PROJECT_NAME} PRIVATE ZYB_RAWMOVE_API=__declspec\(dllexport\))
target_compile_definitions(${PROJECT_NAME} INTERFACE ZYB_RAWMOVE_API=__declspec\(dllimport\))
target_include_directories(${PROJECT_NAME} INTERFACE "${ProjectRootDir}/${CURRENT_FOLDER_NAME}/interface/")

# 安装
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION bin
    ARCHIVE DESTINATION lib
)
install(DIRECTORY
        "${ProjectRootDir}/${CURRENT_FOLDER_NAME}/interface/"
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)

foreach(file IN LISTS ALL_IMPORTED_LOCATION_Debug)
    install(FILES
            $<$<CONFIG:Debug>:${file}>
        DESTINATION bin
    )
endforeach()

foreach(file IN LISTS ALL_IMPORTED_LOCATION_Release)
    install(FILES
            $<$<CONFIG:Release>:${file}>
        DESTINATION bin
    )
endforeach()
