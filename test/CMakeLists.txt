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

# ─── 安装 ─────────────────────────────────────────────────────────────────────
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
install(FILES ${HEADERS} DESTINATION include/ImageSearch)

# 安装运行时 DLL 依赖
Install_Imported_Locations()

# ─── 输出信息 ─────────────────────────────────────────────────────────────────
message(STATUS "=== ImageSearch DLL Configuration ===")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Output dir: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}")
message(STATUS "  C++ standard: ${CMAKE_CXX_STANDARD}")
