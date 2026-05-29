cmake_minimum_required(VERSION 3.8)

include("${CMAKE_CURRENT_LIST_DIR}/ProjectRefrence.cmake")

project(RootProject)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
message("${PROJECT_NAME} --------------------- ffffffff")

# 每次 configure 后自动将 compile_commands.json 复制到根目录（供 clangd 使用）
if(CMAKE_EXPORT_COMPILE_COMMANDS)
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_BINARY_DIR}/compile_commands.json
            ${CMAKE_SOURCE_DIR}/compile_commands.json
    )
endif()

add_subdirectory(Tests/HelloWorld)
add_subdirectory(ArchiveTool)
add_subdirectory(ArchiveToolConsole)

# ArchiveToolUI：dotnet publish → bin/ArchiveToolUI/（等效于 cmake --install）
find_program(DOTNET_EXE dotnet
    HINTS "C:/Program Files/dotnet"
    DOC "dotnet CLI executable")

if(DOTNET_EXE)
    add_custom_target(install_ui
        COMMAND ${DOTNET_EXE} publish
            "${ProjectRootDir}/ArchiveToolUI/ArchiveToolUI.csproj"
            -c Release
        WORKING_DIRECTORY "${ProjectRootDir}"
        COMMENT "Publishing ArchiveToolUI → bin/ArchiveToolUI/"
        VERBATIM
    )
    message(STATUS "install_ui target available (dotnet: ${DOTNET_EXE})")
else()
    message(STATUS "dotnet not found, install_ui target skipped")
endif()
