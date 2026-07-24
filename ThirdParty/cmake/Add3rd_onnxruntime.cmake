# 使用前需设置变量: ProjectName
# 示例:
#   set(ProjectName MyTarget)
#   include(${ProjectRootDir}/ThirdParty/cmake/Add3rd_onnxruntime.cmake)
#
# 注意: 本文件对接的是 ONNX Runtime 官方预编译包 (onnxruntime-win-x64-<ver>.zip)，
# 而非 vcpkg 构建产物。该包不附带任何 CMake config/package 文件，只有
# include/*.h 和 lib/{onnxruntime.dll,.lib, onnxruntime_providers_shared.dll,.lib}，
# 因此这里手动声明 IMPORTED 目标。该包只有一套 Release 产物，Debug/Release 复用同一份。

set(_onnxruntime_root "${ProjectRootDir}/ThirdParty/install/onnxruntime")

message("onnxruntime root == ${_onnxruntime_root}")

if(NOT TARGET onnxruntime::onnxruntime)
    add_library(onnxruntime::onnxruntime SHARED IMPORTED)
    set_target_properties(onnxruntime::onnxruntime PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${_onnxruntime_root}/include"
        IMPORTED_LOCATION_DEBUG   "${_onnxruntime_root}/lib/onnxruntime.dll"
        IMPORTED_IMPLIB_DEBUG     "${_onnxruntime_root}/lib/onnxruntime.lib"
        IMPORTED_LOCATION_RELEASE "${_onnxruntime_root}/lib/onnxruntime.dll"
        IMPORTED_IMPLIB_RELEASE   "${_onnxruntime_root}/lib/onnxruntime.lib"
    )
    set_property(TARGET onnxruntime::onnxruntime APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG RELEASE)
endif()

if(NOT TARGET onnxruntime::providers_shared)
    add_library(onnxruntime::providers_shared SHARED IMPORTED)
    set_target_properties(onnxruntime::providers_shared PROPERTIES
        IMPORTED_LOCATION_DEBUG   "${_onnxruntime_root}/lib/onnxruntime_providers_shared.dll"
        IMPORTED_IMPLIB_DEBUG     "${_onnxruntime_root}/lib/onnxruntime_providers_shared.lib"
        IMPORTED_LOCATION_RELEASE "${_onnxruntime_root}/lib/onnxruntime_providers_shared.dll"
        IMPORTED_IMPLIB_RELEASE   "${_onnxruntime_root}/lib/onnxruntime_providers_shared.lib"
    )
    set_property(TARGET onnxruntime::providers_shared APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG RELEASE)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE onnxruntime::onnxruntime onnxruntime::providers_shared)

Add_Imported_Location(onnxruntime::onnxruntime onnxruntime::providers_shared)
