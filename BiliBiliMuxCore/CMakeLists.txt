cmake_minimum_required(VERSION 3.18)

include("${CMAKE_CURRENT_LIST_DIR}/../ProjectRefrence.cmake")

project(BiliBiliMuxCore)

# 定义库
# add_library(${PROJECT_NAME} SHARED)
add_executable(${PROJECT_NAME})
set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${ProjectRootDir}/bin"
    VS_DEBUGGER_COMMAND "${ProjectRootDir}/bin/${PROJECT_NAME}.exe"
)


# if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
#     target_compile_options(${PROJECT_NAME} PRIVATE
#         -Wall
#         -Wextra
#         -Werror
#     )
# elseif (MSVC)
#     target_compile_options(${PROJECT_NAME} PRIVATE
#         /W4 /WX
#     )
# endif()

if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
endif()



# 三方库
include("${ProjectRootDir}/ThirdParty/cmake/Add3rd_nlohmann_json.cmake")
include("${ProjectRootDir}/ThirdParty/cmake/Add3rd_tbb.cmake")
include("${ProjectRootDir}/ThirdParty/cmake/Add3rd_ffmpeg.cmake")
include("${ProjectRootDir}/ThirdParty/cmake/Add3rd_spdlog.cmake")
include("${ProjectRootDir}/ThirdParty/cmake/Add3rd_curl.cmake")

# 包含目录
target_include_directories(${PROJECT_NAME} PRIVATE
    "${ProjectRootDir}/${CURRENT_FOLDER_NAME}"
    "${ProjectRootDir}/${CURRENT_FOLDER_NAME}/src"
    "${ProjectRootDir}/${CURRENT_FOLDER_NAME}/interface"
)

# 源文件
target_sources_group(${PROJECT_NAME} "src" PRIVATE
    "${ProjectRootDir}/${CURRENT_FOLDER_NAME}/attach.cpp"
    "${ProjectRootDir}/${CURRENT_FOLDER_NAME}/attach.h"
    "${ProjectRootDir}/${CURRENT_FOLDER_NAME}/AvApiWrapper.cpp"
    "${ProjectRootDir}/${CURRENT_FOLDER_NAME}/AvApiWrapper.h"
    "${ProjectRootDir}/${CURRENT_FOLDER_NAME}/BiliApiClient.cpp"
    "${ProjectRootDir}/${CURRENT_FOLDER_NAME}/BiliApiClient.h"
    "${ProjectRootDir}/${CURRENT_FOLDER_NAME}/BiliApiCache.cpp"
    "${ProjectRootDir}/${CURRENT_FOLDER_NAME}/BiliApiCache.h"
    "${ProjectRootDir}/${CURRENT_FOLDER_NAME}/BiliCache.cpp"
    "${ProjectRootDir}/${CURRENT_FOLDER_NAME}/BiliCache.h"
    "${ProjectRootDir}/${CURRENT_FOLDER_NAME}/common.cpp"
    "${ProjectRootDir}/${CURRENT_FOLDER_NAME}/common.h"
    "${ProjectRootDir}/${CURRENT_FOLDER_NAME}/ffmpegMsg.cpp"
    "${ProjectRootDir}/${CURRENT_FOLDER_NAME}/ffmpegMsg.h"
    "${ProjectRootDir}/${CURRENT_FOLDER_NAME}/Logger.cpp"
    "${ProjectRootDir}/${CURRENT_FOLDER_NAME}/Logger.h"
    "${ProjectRootDir}/${CURRENT_FOLDER_NAME}/main.cpp"
    "${ProjectRootDir}/${CURRENT_FOLDER_NAME}/MatchCollector.h"
    "${ProjectRootDir}/${CURRENT_FOLDER_NAME}/MediaMux.cpp"
    "${ProjectRootDir}/${CURRENT_FOLDER_NAME}/MediaMux.h"
    "${ProjectRootDir}/${CURRENT_FOLDER_NAME}/ReadNode.cpp"
    "${ProjectRootDir}/${CURRENT_FOLDER_NAME}/ReadNode.h"
    "${ProjectRootDir}/${CURRENT_FOLDER_NAME}/test.cpp"
    "${ProjectRootDir}/${CURRENT_FOLDER_NAME}/tools.cpp"
    "${ProjectRootDir}/${CURRENT_FOLDER_NAME}/tools.h"
    # BiliBiliMuxCore\main.cpp
)

# target_sources_group(${PROJECT_NAME} "src" PRIVATE
#     "${ProjectRootDir}/${CURRENT_FOLDER_NAME}/src/RawMoveMain.cpp"
#     "${ProjectRootDir}/${CURRENT_FOLDER_NAME}/src/CommonTool.cpp"
#     "${ProjectRootDir}/${CURRENT_FOLDER_NAME}/src/CommonTool.h"
# )

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
# install(FILES
#         # ${ALL_IMPORTED_LOCATION}
#         $<$<CONFIG:Debug>:${ALL_IMPORTED_LOCATION_Debug}>
#         $<$<CONFIG:Release>:${ALL_IMPORTED_LOCATION_Release}>
#     DESTINATION bin
# )

# 统一安装运行时依赖文件
Install_Imported_Locations()
