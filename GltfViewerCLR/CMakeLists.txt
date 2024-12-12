cmake_minimum_required(VERSION 3.8)

include("${CMAKE_CURRENT_LIST_DIR}/../ProjectRefrence.cmake")

project(GltfViewerCLR)

# 定义库
add_library(${PROJECT_NAME} SHARED)
# 设置 C++/CLI 目标属性
set_target_properties(${PROJECT_NAME} PROPERTIES
    # CMAKE_CXX_FLAGS ""
    COMMON_LANGUAGE_RUNTIME ""
    # DOTNET_TARGET_FRAMEWORK "net8.0"
)

# vs调试
set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${ProjectRootDir}/bin"
    VS_DEBUGGER_COMMAND "${ProjectRootDir}/bin/${PROJECT_NAME}.exe"
)

# 三方库


# 包含目录
target_include_directories(${PROJECT_NAME} PRIVATE
    "${ProjectRootDir}/GltfViewerCLR"
    "${ProjectRootDir}/GltfViewerCore/interface"
)

# 源文件
target_sources(${PROJECT_NAME} PRIVATE
    "${ProjectRootDir}/GltfViewerCLR/src/GltfViewerCLRMain.cpp"
)


# 安装
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION bin
    ARCHIVE DESTINATION lib
)
install(FILES
        ${ALL_IMPORTED_LOCATION}
        $<$<CONFIG:Debug>:${ALL_IMPORTED_LOCATION_Debug}>
        $<$<CONFIG:Release>:${ALL_IMPORTED_LOCATION_Release}>
    DESTINATION bin
)