cmake_minimum_required(VERSION 3.8)

include("${CMAKE_CURRENT_LIST_DIR}/../ProjectRefrence.cmake")

project(MAIN_PROJECT)

add_executable(${PROJECT_NAME})

set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${ProjectRootDir}/bin"
    VS_DEBUGGER_COMMAND "${ProjectRootDir}/bin/${PROJECT_NAME}.exe"
)


# 三方库
Add3rd_Glad(${PROJECT_NAME})
Add3rd_glfw3(${PROJECT_NAME})
Add3rd_GLEW(${PROJECT_NAME})
Add3rd_glm(${PROJECT_NAME})
Add3rd_stb_image(${PROJECT_NAME})
# Add3rd_vulkan(${PROJECT_NAME})
Add3rd_bit7z(${PROJECT_NAME})

# 源文件
target_sources(${PROJECT_NAME} PRIVATE
    "${ProjectRootDir}/Test/src/main.cpp"
)

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