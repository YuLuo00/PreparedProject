# 使用前需设置变量: PROJECT_NAME
set(FFmpeg_DIR "${ProjectRootDir}/ThirdParty/ffmpeg/installed/x64-windows/share/ffmpeg/")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${ProjectRootDir}/ThirdParty/ffmpeg/installed/x64-windows/share/ffmpeg/")
message("FFmpeg_DIR == ${FFmpeg_DIR}")

find_package(FFmpeg REQUIRED)

message("
    FFMPEG_FOUND == ${FFMPEG_FOUND}
    FFMPEG_INCLUDE_DIRS == ${FFMPEG_INCLUDE_DIRS}
    FFMPEG_LIBRARY_DIRS == ${FFMPEG_LIBRARY_DIRS}
    FFMPEG_LIBRARIES == ${FFMPEG_LIBRARIES}
")

# 绑定头文件
target_include_directories(${PROJECT_NAME} PRIVATE
    ${FFMPEG_INCLUDE_DIRS}
)

# 绑定库
target_link_directories(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Debug>:${ProjectRootDir}/ThirdParty/ffmpeg/installed/x64-windows/debug/lib/>
    $<$<CONFIG:Release>:${ProjectRootDir}/ThirdParty/ffmpeg/installed/x64-windows/lib>
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    avcodec.lib
    avdevice.lib
    avfilter.lib
    avformat.lib
    avutil.lib
    pkgconf.lib
    swresample.lib
    swscale.lib
)
list(APPEND ALL_IMPORTED_LOCATION_Debug
    ${ProjectRootDir}/ThirdParty/ffmpeg/installed/x64-windows/debug/bin/avcodec-61.dll
    ${ProjectRootDir}/ThirdParty/ffmpeg/installed/x64-windows/debug/bin/avdevice-61.dll
    ${ProjectRootDir}/ThirdParty/ffmpeg/installed/x64-windows/debug/bin/avfilter-10.dll
    ${ProjectRootDir}/ThirdParty/ffmpeg/installed/x64-windows/debug/bin/avformat-61.dll
    ${ProjectRootDir}/ThirdParty/ffmpeg/installed/x64-windows/debug/bin/avutil-59.dll
    ${ProjectRootDir}/ThirdParty/ffmpeg/installed/x64-windows/debug/bin/pkgconf-3.dll
    ${ProjectRootDir}/ThirdParty/ffmpeg/installed/x64-windows/debug/bin/swresample-5.dll
    ${ProjectRootDir}/ThirdParty/ffmpeg/installed/x64-windows/debug/bin/swscale-8.dll
)
list(APPEND ALL_IMPORTED_LOCATION_Release
    ${ProjectRootDir}/ThirdParty/ffmpeg/installed/x64-windows/bin/avcodec-61.dll
    ${ProjectRootDir}/ThirdParty/ffmpeg/installed/x64-windows/bin/avdevice-61.dll
    ${ProjectRootDir}/ThirdParty/ffmpeg/installed/x64-windows/bin/avfilter-10.dll
    ${ProjectRootDir}/ThirdParty/ffmpeg/installed/x64-windows/bin/avformat-61.dll
    ${ProjectRootDir}/ThirdParty/ffmpeg/installed/x64-windows/bin/avutil-59.dll
    ${ProjectRootDir}/ThirdParty/ffmpeg/installed/x64-windows/bin/pkgconf-3.dll
    ${ProjectRootDir}/ThirdParty/ffmpeg/installed/x64-windows/bin/swresample-5.dll
    ${ProjectRootDir}/ThirdParty/ffmpeg/installed/x64-windows/bin/swscale-8.dll
)
