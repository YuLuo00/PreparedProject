cmake_minimum_required(VERSION 3.20)
project(ImageSearch VERSION 1.0.0 LANGUAGES CXX)

# ─── 引入项目基准配置 ─────────────────────────────────────────────────────────
include(${CMAKE_CURRENT_SOURCE_DIR}/../ProjectRefrence.cmake)

# ─── 源文件 ───────────────────────────────────────────────────────────────────
set(SOURCES
    src/ImageSearch.cpp
    src/Database.cpp
    src/FaissIndex.cpp
    src/HttpClient.cpp
    src/HttpServer.cpp
)

set(HEADERS
    src/ImageSearch.h
    src/Database.h
    src/FaissIndex.h
    src/HttpClient.h
    src/HttpServer.h
)

# ─── 创建 DLL ─────────────────────────────────────────────────────────────────
add_library(${PROJECT_NAME} SHARED ${SOURCES} ${HEADERS})

# 导出宏
target_compile_definitions(${PROJECT_NAME} PRIVATE
    IMAGESEARCH_EXPORTS
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

# C++17
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

# MSVC 编码设置
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8 /W3 /wd4996)
endif()

# ─── 引入三方库 ───────────────────────────────────────────────────────────────

# SQLite3
include(${ProjectRootDir}/ThirdParty/cmake/Add3rd_sqlite3.cmake)

# nlohmann/json
include(${ProjectRootDir}/ThirdParty/cmake/Add3rd_nlohmann_json.cmake)

# cpp-httplib
include(${ProjectRootDir}/ThirdParty/cmake/Add3rd_cpp_httplib.cmake)

# Faiss
include(${ProjectRootDir}/ThirdParty/cmake/Add3rd_faiss.cmake)

# ─── 测试可执行程序 ───────────────────────────────────────────────────────────

add_executable(ImageSearchTest
    test/test_main.cpp
    src/Database.cpp
    src/FaissIndex.cpp
    src/HttpClient.cpp
    src/HttpServer.cpp
    src/ImageSearch.cpp
)

# 测试程序直接编译所有源文件（不链接 DLL），定义 IMAGESEARCH_EXPORTS 使函数可见
target_compile_definitions(ImageSearchTest PRIVATE
    IMAGESEARCH_EXPORTS
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

target_compile_features(ImageSearchTest PRIVATE cxx_std_17)

if(MSVC)
    target_compile_options(ImageSearchTest PRIVATE /utf-8 /W3 /wd4996)
endif()

# 直接链接已经 find_package 过的目标（DLL 目标已经 include 过 cmake 文件）
target_link_libraries(ImageSearchTest PRIVATE
    unofficial::sqlite3::sqlite3
    nlohmann_json::nlohmann_json
    httplib::httplib
    faiss
)

# ─── 安装 ─────────────────────────────────────────────────────────────────────
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
install(FILES ${HEADERS} DESTINATION include/ImageSearch)

# 安装运行时 DLL 依赖
Install_Imported_Locations()

# 额外安装 cpp-httplib 的 brotli DLL（httplib 依赖，但未被 Add_Imported_Location 捕获）
install(FILES
    "${ProjectRootDir}/ThirdParty/install/cpp-httplib/installed/x64-windows/bin/brotlicommon.dll"
    "${ProjectRootDir}/ThirdParty/install/cpp-httplib/installed/x64-windows/bin/brotlidec.dll"
    "${ProjectRootDir}/ThirdParty/install/cpp-httplib/installed/x64-windows/bin/brotlienc.dll"
    DESTINATION bin
)

# 额外安装 faiss 的间接依赖 DLL（openblas/lapack 及 MinGW 运行时，未被 Add_Imported_Location 捕获）
set(_faiss_debug_bin "${ProjectRootDir}/ThirdParty/install/faiss/installed/x64-windows/debug/bin")
set(_faiss_release_bin "${ProjectRootDir}/ThirdParty/install/faiss/installed/x64-windows/bin")
set(_faiss_mingw_dlls
    libgfortran-5.dll
    libgcc_s_seh-1.dll
    libquadmath-0.dll
    libwinpthread-1.dll
)

install(FILES "${_faiss_debug_bin}/openblas.dll" "${_faiss_debug_bin}/liblapack.dll"
    DESTINATION bin CONFIGURATIONS Debug)
install(FILES "${_faiss_release_bin}/openblas.dll" "${_faiss_release_bin}/liblapack.dll"
    DESTINATION bin CONFIGURATIONS Release)

foreach(_dll ${_faiss_mingw_dlls})
    install(FILES "${_faiss_debug_bin}/${_dll}"   DESTINATION bin CONFIGURATIONS Debug)
    install(FILES "${_faiss_release_bin}/${_dll}" DESTINATION bin CONFIGURATIONS Release)
endforeach()

# ─── DLL Full Test（链接 import lib，不编译 DLL 源码）────────────────────────

add_executable(ImageSearchDllTest
    test/dll_full_test.cpp
)

target_compile_definitions(ImageSearchDllTest PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

target_compile_features(ImageSearchDllTest PRIVATE cxx_std_17)

if(MSVC)
    target_compile_options(ImageSearchDllTest PRIVATE /utf-8 /W3 /wd4996)
endif()

# 链接 DLL 导入库（不编译源码）
target_link_libraries(ImageSearchDllTest PRIVATE
    ${ProjectRootDir}/lib/ImageSearch.lib
    nlohmann_json::nlohmann_json
    httplib::httplib
)

# 包含 DLL 公共头文件目录（include/ImageSearch/ImageSearch.h）
target_include_directories(ImageSearchDllTest PRIVATE
    ${ProjectRootDir}/include
)

# 安装 DLL Full Test 可执行文件
install(TARGETS ImageSearchDllTest
    RUNTIME DESTINATION bin
)

# ─── 输出信息 ─────────────────────────────────────────────────────────────────
message(STATUS "=== ImageSearch DLL Configuration ===")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Output dir: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}")
message(STATUS "  C++ standard: ${CMAKE_CXX_STANDARD}")
