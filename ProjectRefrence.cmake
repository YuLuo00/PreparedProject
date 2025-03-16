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
macro(Add_Interface_Imported_Location ProjectName)
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
endmacro()

macro(Add_Imported_Location ProjectName)
    get_target_property(_locs ${ProjectName} IMPORTED_LOCATION_DEBUG)
    if(NOT _locs STREQUAL "_locs-NOTFOUND")
    message(STATUS "${ProjectName} >>>> ${_locs}")
        list(APPEND ALL_IMPORTED_LOCATION_Debug ${_locs})
    endif()

    get_target_property(_locs ${ProjectName} IMPORTED_LOCATION_RELEASE)
    if(NOT _locs STREQUAL "_locs-NOTFOUND")
        message(STATUS "${ProjectName} >>>> ${_locs}")
        list(APPEND ALL_IMPORTED_LOCATION_Release ${_locs})
    endif()
endmacro()

# -------------------------------------------------------------------------------- 导入三方库 -------------------------------------

