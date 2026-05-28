project(ArchiveToolConsole)

add_executable(${PROJECT_NAME})

target_sources_group(${PROJECT_NAME} "src" PRIVATE
    "${ProjectRootDir}/ArchiveToolConsole/main.cpp"
)

target_link_libraries(${PROJECT_NAME} PRIVATE ArchiveTool)

target_include_directories(${PROJECT_NAME} PRIVATE
    "${ProjectRootDir}/ArchiveTool/interface"
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${ProjectRootDir}/bin"
)
