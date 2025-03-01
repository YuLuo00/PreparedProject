cmake_minimum_required(VERSION 3.18)

include("${CMAKE_CURRENT_LIST_DIR}/../ProjectRefrence.cmake")

project(${CURRENT_FOLDER_NAME})

# 定义库
# add_library(${PROJECT_NAME} SHARED)
add_executable(${PROJECT_NAME})
# add_executable(${PROJECT_NAME})
set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${ProjectRootDir}/bin"
    VS_DEBUGGER_COMMAND "${ProjectRootDir}/bin/${PROJECT_NAME}.exe"
)

# 三方库
Add3rd_nlohmann_json(${PROJECT_NAME})
Add3rd_fmt(${PROJECT_NAME})


# 包含目录
target_include_directories(${PROJECT_NAME} PRIVATE
    "${ProjectRootDir}/${CURRENT_FOLDER_NAME}"
    "${ProjectRootDir}/${CURRENT_FOLDER_NAME}/src"
    "${ProjectRootDir}/${CURRENT_FOLDER_NAME}/interface"
    "${ProjectRootDir}/DllModule/interface"
)

# 源文件
target_sources_group(${PROJECT_NAME} "src" PRIVATE
    "${ProjectRootDir}/${CURRENT_FOLDER_NAME}/src/CommonTool.cpp"
    "${ProjectRootDir}/${CURRENT_FOLDER_NAME}/src/CommonTool.h"
    "${ProjectRootDir}/${CURRENT_FOLDER_NAME}/src/main.cpp"
    "${ProjectRootDir}/${CURRENT_FOLDER_NAME}/src/GltfRender.cpp"
    "${ProjectRootDir}/${CURRENT_FOLDER_NAME}/src/GLGeometry.h"
    "${ProjectRootDir}/${CURRENT_FOLDER_NAME}/src/GltfRenderGLCore.cpp"
    "${ProjectRootDir}/${CURRENT_FOLDER_NAME}/src/GltfRenderGLCore.h"
)
target_sources_group(${PROJECT_NAME} "interface" PRIVATE
    "${ProjectRootDir}/${CURRENT_FOLDER_NAME}/interface/GltfRender.h"
)

# dll导出宏
target_compile_definitions(${PROJECT_NAME} PRIVATE ZYB_GLTFVIEWER_API=__declspec\(dllexport\))
target_compile_definitions(${PROJECT_NAME} INTERFACE ZYB_GLTFVIEWER_API=__declspec\(dllimport\))

# 导出属性
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
# 安装 - 依赖库
install(FILES ${ALL_IMPORTED_LOCATION} DESTINATION bin)
foreach(_location ${ALL_IMPORTED_LOCATION_Debug})
    install(FILES $<$<CONFIG:Debug>:${_location}> DESTINATION bin)
endforeach()
foreach(_location ${ALL_IMPORTED_LOCATION_Release})
    install(FILES $<$<CONFIG:Release>:${_location}> DESTINATION bin)
endforeach()

