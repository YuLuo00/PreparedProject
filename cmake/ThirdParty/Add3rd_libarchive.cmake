# 使用前需设置变量: ProjectName
# 示例:
#   set(ProjectName MyTarget)
#   include(${ProjectRootDir}/cmake/ThirdParty/libarchive.cmake)

message("LibArchive == ${ProjectRootDir}/ThirdParty/libarchive/installed/x64-windows/")
set(CMAKE_INCLUDE_PATH "${ProjectRootDir}/ThirdParty/libarchive/installed/x64-windows/include")
set(CMAKE_LIBRARY_PATH "${ProjectRootDir}/ThirdParty/libarchive/installed/x64-windows/lib")

find_package(LibArchive REQUIRED)
target_link_libraries(${ProjectName} PRIVATE LibArchive::LibArchive)

install(DIRECTORY
    $<$<CONFIG:Debug>:${ProjectRootDir}/ThirdParty/libarchive/installed/x64-windows/debug/bin/>
    $<$<CONFIG:Release>:${ProjectRootDir}/ThirdParty/libarchive/installed/x64-windows/bin/>
    DESTINATION bin
    FILES_MATCHING PATTERN "*.dll"
)
