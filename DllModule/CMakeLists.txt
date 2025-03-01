cmake_minimum_required(VERSION 3.8)

include("${CMAKE_CURRENT_LIST_DIR}/../ProjectRefrence.cmake")

project(DllModule)

# 定义库
add_library(${PROJECT_NAME} SHARED)

# vs调试
set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${ProjectRootDir}/bin"
    VS_DEBUGGER_COMMAND "${ProjectRootDir}/bin/${PROJECT_NAME}.exe"
)

# 三方库

# 附加宏定义
target_compile_definitions(${PROJECT_NAME} PRIVATE
    DLL_MODULE_EXPORT ""
)

# 包含目录
target_include_directories(${PROJECT_NAME} PRIVATE
    "${ProjectRootDir}/${CURRENT_FOLDER_NAME}"
    "${ProjectRootDir}/${CURRENT_FOLDER_NAME}/interface"
)

# 源文件
target_sources(${PROJECT_NAME} PRIVATE
    "${ProjectRootDir}/${CURRENT_FOLDER_NAME}/src/GltfViewerCLRMain.cpp"
    "${ProjectRootDir}/${CURRENT_FOLDER_NAME}/interface/DllModuleInterface.h"
)


# 安装
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION bin
    ARCHIVE DESTINATION lib
)

#安装头文件
install(

)

install(FILES
        ${ALL_IMPORTED_LOCATION}
        $<$<CONFIG:Debug>:${ALL_IMPORTED_LOCATION_Debug}>
        $<$<CONFIG:Release>:${ALL_IMPORTED_LOCATION_Release}>
    DESTINATION bin
)