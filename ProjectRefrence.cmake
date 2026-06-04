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
    foreach(ProjectName IN ITEMS ${ARGV})
        message(STATUS -----${ProjectName})
        if(TARGET ${ProjectName})
            get_target_property(_locs ${ProjectName} INTERFACE_IMPORTED_LOCATION)
            if(_locs AND NOT _locs STREQUAL "_locs-NOTFOUND")
                message(STATUS "${ProjectName} INTERFACE_IMPORTED_LOCATION >>>>  ${_locs}")
                list(APPEND ALL_IMPORTED_LOCATION ${_locs})
            endif()
            
            get_target_property(_locs ${ProjectName} INTERFACE_IMPORTED_LOCATION_DEBUG)
            if(_locs AND NOT _locs STREQUAL "_locs-NOTFOUND")
                message(STATUS "${ProjectName} INTERFACE_IMPORTED_LOCATION_DEBUG >>>>  ${_locs}")
                list(APPEND ALL_IMPORTED_LOCATION_Debug ${_locs})
            endif()

            get_target_property(_locs ${ProjectName} INTERFACE_IMPORTED_LOCATION_RELEASE)
            if(_locs AND NOT _locs STREQUAL "_locs-NOTFOUND")
                message(STATUS "${ProjectName} INTERFACE_IMPORTED_LOCATION_RELEASE >>>>  ${_locs}")
                list(APPEND ALL_IMPORTED_LOCATION_Release ${_locs})
            endif()
        else()
            message(STATUS "${ProjectName} is not a valid target, skipping")
        endif()
    endforeach()
endmacro()


macro(Add_Imported_Location)
    foreach(ProjectName IN ITEMS ${ARGV})
        if(TARGET ${ProjectName})
            get_target_property(_locs ${ProjectName} IMPORTED_LOCATION_DEBUG)
            if(_locs AND NOT _locs STREQUAL "_locs-NOTFOUND")
                message(STATUS "${ProjectName} IMPORTED_LOCATION_DEBUG >>>> ${_locs}")
                list(APPEND ALL_IMPORTED_LOCATION_Debug ${_locs})
            endif()

            get_target_property(_locs ${ProjectName} IMPORTED_LOCATION_RELEASE)
            if(_locs AND NOT _locs STREQUAL "_locs-NOTFOUND")
                message(STATUS "${ProjectName} IMPORTED_LOCATION_RELEASE >>>> ${_locs}")
                list(APPEND ALL_IMPORTED_LOCATION_Release ${_locs})
            endif()
        else()
            message(STATUS "${ProjectName} is not a valid target, skipping")
        endif()
    endforeach()
endmacro()

# 安装运行时依赖 DLL（遍历 ALL_IMPORTED_LOCATION_Debug/Release，逐个 install）
macro(Install_Imported_Locations)
    foreach(_dll ${ALL_IMPORTED_LOCATION})
        if(NOT _dll STREQUAL "_locs-NOTFOUND")
            message(STATUS "${ProjectName} ALL_IMPORTED_LOCATION install >>>>  ${_dll}")
            install(FILES ${_dll} DESTINATION bin CONFIGURATIONS)
        else()
            message(STATUS "${ProjectName} ALL_IMPORTED_LOCATION NOTFOUND >>>>  ${_dll}")
        endif()
    endforeach()

    foreach(_dll ${ALL_IMPORTED_LOCATION_Debug})
        if(NOT _dll STREQUAL "_locs-NOTFOUND")
            message(STATUS "${ProjectName} ALL_IMPORTED_LOCATION_Debug install >>>>  ${_dll}")
            install(FILES ${_dll} DESTINATION bin CONFIGURATIONS Debug)
        else()
            message(STATUS "${ProjectName} ALL_IMPORTED_LOCATION_Debug NOTFOUND >>>>  ${_dll}")
        endif()
    endforeach()

    foreach(_dll ${ALL_IMPORTED_LOCATION_Release})
        if(NOT _dll STREQUAL "_locs-NOTFOUND")
            message(STATUS "${ProjectName} ALL_IMPORTED_LOCATION_Release install >>>>  ${_dll}")
            install(FILES ${_dll} DESTINATION bin CONFIGURATIONS Release)
        else()
            message(STATUS "${ProjectName} ALL_IMPORTED_LOCATION_Release NOTFOUND >>>>  ${_dll}")
        endif()
    endforeach()
endmacro()


# -------------------------------------------------------------------------------- 导入三方库 -------------------------------------
# 各第三方库已拆分为独立 .cmake 文件，位于 cmake/ThirdParty/ 目录下。
# 使用方式（在子项目 CMakeLists.txt 中）：
#
#   set(ProjectName MyTarget)
#   add_executable(${ProjectName} main.cpp)
#   include(${ProjectRootDir}/cmake/ThirdParty/Add3rd_spdlog.cmake)
#   include(${ProjectRootDir}/cmake/ThirdParty/Add3rd_fmt.cmake)
#   # ... 按需 include 其他库
#
# 可用的库文件：
#   cmake/ThirdParty/Add3rd_spdlog.cmake
#   cmake/ThirdParty/Add3rd_fmt.cmake
#   cmake/ThirdParty/Add3rd_nlohmann_json.cmake
#   cmake/ThirdParty/Add3rd_libarchive.cmake
#   cmake/ThirdParty/Add3rd_7zextra.cmake
#   cmake/ThirdParty/Add3rd_bit7z.cmake
#   cmake/ThirdParty/Add3rd_vulkan.cmake
#   cmake/ThirdParty/Add3rd_freeimage.cmake
#   cmake/ThirdParty/Add3rd_glm.cmake
#   cmake/ThirdParty/Add3rd_glad.cmake
#   cmake/ThirdParty/Add3rd_glfw3.cmake
#   cmake/ThirdParty/Add3rd_glew.cmake
#   cmake/ThirdParty/Add3rd_opencv.cmake
#   cmake/ThirdParty/Add3rd_sqlite3.cmake
#   cmake/ThirdParty/Add3rd_opencv411.cmake
#   cmake/ThirdParty/Add3rd_tbb.cmake
#   cmake/ThirdParty/Add3rd_ffmpeg.cmake
