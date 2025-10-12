# 项目基准路径
set(ProjectRootDir ${CMAKE_CURRENT_LIST_DIR})
message("ProjectRootDir == ${ProjectRootDir}")
set(ALL_IMPORTED_LOCATION "")
set(ALL_IMPORTED_LOCATION_Debug "")
set(ALL_IMPORTED_LOCATION_Release "")

#快捷变量：${CURRENT_FOLDER_NAME} 当前文件夹名字
    # 获取当前文件夹的完整路径
    set(CURRENT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
    # 提取文件夹名
    get_filename_component(CURRENT_FOLDER_NAME ${CURRENT_DIR} NAME)
    # 打印文件夹名（可选）
    message(STATUS "Current Directory Name: ${CURRENT_FOLDER_NAME}")

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)

# 设置解决方案的输出路径
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${ProjectRootDir}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${ProjectRootDir}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${ProjectRootDir}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${ProjectRootDir}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${ProjectRootDir}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${ProjectRootDir}/bin)

# 设置仅生成 Debug 和 Release 配置
set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "Limited build configurations" FORCE)

# 添加分组
macro(target_sources_group TargetName GroupName PERMISSION)
    target_sources(${TargetName} ${PERMISSION} ${ARGN})
    source_group(${GroupName} FILES ${ARGN})
endmacro()

# 添加运行时依赖文件
macro(Add_Interface_Imported_Location)
    message(STATUS -----${ProjectName})
    foreach(ProjectName IN ITEMS ${ARGV})
        message(STATUS -----${ProjectName})
        get_target_property(_locs ${ProjectName} INTERFACE_IMPORTED_LOCATION)
        if(NOT _locs STREQUAL "_locs-NOTFOUND")
            message(STATUS "MY_VAR is _locs-NOTFOUND")
            list(APPEND ALL_IMPORTED_LOCATION ${_locs})
        endif()
        
        get_target_property(_locs ${ProjectName} INTERFACE_IMPORTED_LOCATION_DEBUG)
        if(NOT _locs STREQUAL "_locs-NOTFOUND")
            message(STATUS "MY_VAR is _locs-NOTFOUND")
            list(APPEND ALL_IMPORTED_LOCATION_Debug ${_locs})
        endif()

        get_target_property(_locs ${ProjectName} INTERFACE_IMPORTED_LOCATION_RELEASE)
        if(NOT _locs STREQUAL "_locs-NOTFOUND")
            message(STATUS "MY_VAR is _locs-NOTFOUND")
            list(APPEND ALL_IMPORTED_LOCATION_Release ${_locs})
        endif()

        get_target_property(_locs ${ProjectName} IMPORTED_LOCATION_DEBUG)
        if(NOT _locs STREQUAL "_locs-NOTFOUND")
            message(STATUS "MY_VAR is _locs-NOTFOUND")
            list(APPEND ALL_IMPORTED_LOCATION_Debug ${_locs})
        endif()

        get_target_property(_locs ${ProjectName} IMPORTED_LOCATION_RELEASE)
        if(NOT _locs STREQUAL "_locs-NOTFOUND")
            message(STATUS "MY_VAR is _locs-NOTFOUND")
            list(APPEND ALL_IMPORTED_LOCATION_Release ${_locs})
        endif()
    endforeach()
endmacro()





    # # 更新变量作用域
    # set(ALL_IMPORTED_LOCATION_Debug ${ALL_IMPORTED_LOCATION_Debug} PARENT_SCOPE)
    # set(ALL_IMPORTED_LOCATION_Release ${ALL_IMPORTED_LOCATION_Release} PARENT_SCOPE)


# -------------------------------------------------------------------------------- 导入三方库 -------------------------------------

macro(Add3rd_fmt ProjectName)
    set(fmt_DIR "${ProjectRootDir}/ThirdParty/fmt/installed/x64-windows/share/fmt/")
    message("fmt_DIR == ${fmt_DIR}")
    # this is heuristically generated, and may not be correct
    find_package(fmt CONFIG REQUIRED)
    target_link_libraries(${ProjectName} PRIVATE fmt::fmt-header-only)

    Add_Interface_Imported_Location(fmt::fmt-header-only)
endmacro()

macro(Add3rd_sqlite3 ProjectName)
    set(unofficial-sqlite3_DIR "${ProjectRootDir}/ThirdParty/sqlite3/installed/x64-windows/share/unofficial-sqlite3/")
    message("unofficial-sqlite3_DIR == ${unofficial-sqlite3_DIR}")

    find_package(unofficial-sqlite3 CONFIG REQUIRED)
    target_link_libraries(${ProjectName} PRIVATE unofficial::sqlite3::sqlite3)

    Add_Interface_Imported_Location(unofficial::sqlite3::sqlite3)
endmacro()

macro(Add3rd_OpenCV ProjectName)
    set(Protobuf_DIR "${ProjectRootDir}/ThirdParty/opencv/installed/x64-windows/share/protobuf/")
    set(quirc_DIR "${ProjectRootDir}/ThirdParty/opencv/installed/x64-windows/share/quirc/")
    set(OpenCV_DIR "${ProjectRootDir}/ThirdParty/opencv/installed/x64-windows/share/opencv4/")
    
    set(TIFF_INCLUDE_DIR "${ProjectRootDir}/ThirdParty/opencv/installed/x64-windows/include/")
    set(TIFF_LIBRARY "${ProjectRootDir}/ThirdParty/opencv/installed/x64-windows/bin/")
    list(APPEND ALL_IMPORTED_LOCATION_Debug "${ProjectRootDir}/ThirdParty/opencv/installed/x64-windows/debug/bin/tiffd.dll")
    list(APPEND ALL_IMPORTED_LOCATION_Release "${ProjectRootDir}/ThirdParty/opencv/installed/x64-windows/bin/tiff.dll")
    message("OpenCV_DIR == ${OpenCV_DIR}")

    set(libjpeg-turbo_DIR "${ProjectRootDir}/ThirdParty/opencv/installed/x64-windows/share/libjpeg-turbo/")
    set(WebP_DIR "${ProjectRootDir}/ThirdParty/opencv/installed/x64-windows/share/WebP/")
    set(libpng_DIR "${ProjectRootDir}/ThirdParty/opencv/installed/x64-windows/share/libpng/")
    find_package(libpng CONFIG REQUIRED)
    find_package(libjpeg-turbo CONFIG REQUIRED)
    find_package(WebP CONFIG REQUIRED)
    # this is heuristically generated, and may not be correct
    find_package(OpenCV CONFIG REQUIRED)
    

    # note: 10 additional targets are not displayed.
    target_link_libraries(${ProjectName} PRIVATE
        opencv_ml opencv_dnn opencv_core opencv_flann opencv_imgcodecs opencv_imgproc
        libjpeg-turbo::jpeg
        WebP::webp WebP::webpdecoder WebP::webpdemux WebP::sharpyuv
        png
    )
    Add_Interface_Imported_Location(
        opencv_ml opencv_dnn opencv_core opencv_flann opencv_imgcodecs opencv_imgproc
        libjpeg-turbo::jpeg
        WebP::webp WebP::webpdecoder WebP::webpdemux WebP::sharpyuv
        png
    )
endmacro()

macro(Add3rd_sqlite3 ProjectName)
    set(unofficial-sqlite3_DIR "${ProjectRootDir}/ThirdParty/sqlite3/installed/x64-windows/share/unofficial-sqlite3/")
    message("unofficial-sqlite3_DIR == ${unofficial-sqlite3_DIR}")

    find_package(unofficial-sqlite3 CONFIG REQUIRED)
    target_link_libraries(${ProjectName} PRIVATE unofficial::sqlite3::sqlite3)
    
    Add_Interface_Imported_Location(unofficial::sqlite3::sqlite3)
endmacro()

macro(Add3rd_OpenCV411 ProjectName)
    set(Protobuf_DIR "${ProjectRootDir}/ThirdParty/opencv411/installed/x64-windows/share/protobuf/")
    set(quirc_DIR "${ProjectRootDir}/ThirdParty/opencv411/installed/x64-windows/share/quirc/")
    set(OpenCV_DIR "${ProjectRootDir}/ThirdParty/opencv411/installed/x64-windows/share/opencv4/")
    
    set(TIFF_INCLUDE_DIR "${ProjectRootDir}/ThirdParty/opencv411/installed/x64-windows/include/")
    set(TIFF_LIBRARY "${ProjectRootDir}/ThirdParty/opencv411/installed/x64-windows/bin/")
    list(APPEND ALL_IMPORTED_LOCATION_Debug "${ProjectRootDir}/ThirdParty/opencv411/installed/x64-windows/debug/bin/tiffd.dll")
    list(APPEND ALL_IMPORTED_LOCATION_Release "${ProjectRootDir}/ThirdParty/opencv411/installed/x64-windows/bin/tiffd.dll")
    message("OpenCV_DIR == ${OpenCV_DIR}")

    set(libjpeg-turbo_DIR "${ProjectRootDir}/ThirdParty/opencv411/installed/x64-windows/share/libjpeg-turbo/")
    set(WebP_DIR "${ProjectRootDir}/ThirdParty/opencv411/installed/x64-windows/share/WebP/")
    set(libpng_DIR "${ProjectRootDir}/ThirdParty/opencv411/installed/x64-windows/share/libpng/")
    find_package(libpng CONFIG REQUIRED)
    find_package(libjpeg-turbo CONFIG REQUIRED)
    find_package(WebP CONFIG REQUIRED)
    # this is heuristically generated, and may not be correct
    find_package(OpenCV CONFIG REQUIRED)
    

    # note: 10 additional targets are not displayed.
    target_link_libraries(${ProjectName} PRIVATE 
        opencv_ml opencv_dnn opencv_core opencv_flann opencv_imgcodecs opencv_imgproc opencv_highgui opencv_videoio
        libjpeg-turbo::jpeg
        WebP::webp WebP::webpdecoder WebP::webpdemux WebP::sharpyuv WebP::libwebpmux
        png
    )
    Add_Interface_Imported_Location(
        opencv_ml opencv_dnn opencv_core opencv_flann opencv_imgcodecs opencv_imgproc opencv_highgui opencv_videoio
        libjpeg-turbo::jpeg
        WebP::webp WebP::webpdecoder WebP::webpdemux WebP::sharpyuv WebP::libwebpmux
        png
    )
endmacro()