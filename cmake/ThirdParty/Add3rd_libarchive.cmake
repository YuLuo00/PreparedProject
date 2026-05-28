if(MINGW)
    set(CMAKE_INCLUDE_PATH "${ProjectRootDir}/ThirdParty/libarchive/installed/x64-mingw-dynamic/include")
    set(CMAKE_LIBRARY_PATH "${ProjectRootDir}/ThirdParty/libarchive/installed/x64-mingw-dynamic/lib")
    message("LibArchive (MinGW) == ${ProjectRootDir}/ThirdParty/libarchive/installed/x64-mingw-dynamic/")
else()
    set(CMAKE_INCLUDE_PATH "${ProjectRootDir}/ThirdParty/libarchive/installed/x64-windows/include")
    set(CMAKE_LIBRARY_PATH "${ProjectRootDir}/ThirdParty/libarchive/installed/x64-windows/lib")
    message("LibArchive == ${ProjectRootDir}/ThirdParty/libarchive/installed/x64-windows/")
endif()

find_package(LibArchive REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE LibArchive::LibArchive)

install(DIRECTORY
    $<$<CONFIG:Debug>:${ProjectRootDir}/ThirdParty/libarchive/installed/x64-windows/debug/bin/>
    $<$<CONFIG:Release>:${ProjectRootDir}/ThirdParty/libarchive/installed/x64-windows/bin/>
    DESTINATION bin
    FILES_MATCHING PATTERN "*.dll"
)
