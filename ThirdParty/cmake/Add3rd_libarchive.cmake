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

# FindLibArchive 只记录 .dll.a 导入库，不记录运行时 DLL 路径，需手动收集
if(MINGW)
    file(GLOB _libarchive_debug_dlls
        "${ProjectRootDir}/ThirdParty/libarchive/installed/x64-mingw-dynamic/debug/bin/*.dll")
    file(GLOB _libarchive_release_dlls
        "${ProjectRootDir}/ThirdParty/libarchive/installed/x64-mingw-dynamic/bin/*.dll")
else()
    file(GLOB _libarchive_debug_dlls
        "${ProjectRootDir}/ThirdParty/libarchive/installed/x64-windows/debug/bin/*.dll")
    file(GLOB _libarchive_release_dlls
        "${ProjectRootDir}/ThirdParty/libarchive/installed/x64-windows/bin/*.dll")
endif()

list(APPEND ALL_IMPORTED_LOCATION_Debug   ${_libarchive_debug_dlls})
list(APPEND ALL_IMPORTED_LOCATION_Release ${_libarchive_release_dlls})



