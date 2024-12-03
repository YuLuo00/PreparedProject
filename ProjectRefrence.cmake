# 项目基准路径
set(ProjectRootDir ${CMAKE_CURRENT_LIST_DIR})
message("ProjectRootDir == ${ProjectRootDir}")
set(ALL_IMPORTED_LOCATION "")
set(ALL_IMPORTED_LOCATION_Debug "")
set(ALL_IMPORTED_LOCATION_Release "")

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
    target_sources_group(${TargetName} ${PERMISSION} ${ARGN})
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

# -------------------------------------------------------------------------------- 导入三方库 -------------------------------------

# macro(Add3rd_ ProjectName)
# endmacro()
macro(Add3rd_ ProjectName)
    set(_DIR "${ProjectRootDir}/ThirdParty/")
    message("_DIR == ${_DIR}")
    # this is heuristically generated, and may not be correct
    find_package( CONFIG REQUIRED)
    target_link_libraries(${ProjectName} PRIVATE )

    Add_Interface_Imported_Location()
endmacro()

macro(Add3rd_glm ProjectName)
    set(glm_DIR "${ProjectRootDir}/ThirdParty/GLM/installed/x64-windows/share/glm")
    message("glm_DIR == ${glm_DIR}")
    # this is heuristically generated, and may not be correct
    find_package(glm CONFIG REQUIRED)
    target_link_libraries(${ProjectName} PRIVATE glm::glm)

    Add_Interface_Imported_Location(glm::glm)
endmacro()

macro(Add3rd_Glad ProjectName)
    set(glad_DIR "${ProjectRootDir}/ThirdParty/glad/installed/x64-windows/share/glad")
    message("glad_DIR == ${glad_DIR}")
    # this is heuristically generated, and may not be correct
    find_package(glad CONFIG REQUIRED)
    target_link_libraries(${ProjectName} PRIVATE glad::glad)
    Add_Interface_Imported_Location(glad::glad)
endmacro()

macro(Add3rd_glfw3 ProjectName)
    set(glfw3_DIR "${ProjectRootDir}/ThirdParty/GLFW3/installed/x64-windows/share/glfw3")
    message("glfw3_DIR == ${glfw3_DIR}")
    # this is heuristically generated, and may not be correct
    find_package(glfw3 CONFIG REQUIRED)
    target_link_libraries(${ProjectName} PRIVATE glfw)

    Add_Interface_Imported_Location(glfw)
endmacro()


macro(Add3rd_GLEW ProjectName)
    set(GLEW_DIR "${ProjectRootDir}/ThirdParty/GLEW/installed/x64-windows/share/glew")
    message("GLEW_DIR == ${GLEW_DIR}")
    # this is heuristically generated, and may not be correct
    find_package(GLEW REQUIRED)
    target_link_libraries(${ProjectName} PRIVATE GLEW::GLEW)

    Add_Interface_Imported_Location(GLEW::GLEW)
endmacro()