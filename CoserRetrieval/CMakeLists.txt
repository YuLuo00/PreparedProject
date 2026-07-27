cmake_minimum_required(VERSION 3.20)
project(CoserRetrieval VERSION 1.0.0 LANGUAGES CXX)

# ─── 引入项目基准配置 ─────────────────────────────────────────────────────────
include(${CMAKE_CURRENT_SOURCE_DIR}/../ProjectRefrence.cmake)

# ─── 核心库源文件（L1~L4） ────────────────────────────────────────────────────
set(CORE_SOURCES
    src/L1/ConfigManager.cpp
    src/L2/FaissFlatIpIndex.cpp
    src/L2/MetadataStore.cpp
    src/L2/PHashIndex.cpp
    src/L3/ScrfdFaceDetector.cpp
    src/L3/ArcFaceExtractor.cpp
    src/L3/FaceAlign.cpp
    src/L3/PhotoAuthenticityChecker.cpp
    src/L3/PHasher.cpp
    src/L3/OrbCropMatcher.cpp
    src/L3/YoloPoseDetector.cpp
    src/L3/DinoV2Extractor.cpp
    src/L4/FaceRecognitionPipeline.cpp
    src/L4/ImageMatchPipeline.cpp
    src/L4/ClothingRecognitionPipeline.cpp
    src/L5/ResultFusion.cpp
    src/L5/RetrievalOrchestrator.cpp
)

set(CORE_HEADERS
    src/L1/ConfigManager.h
    src/L2/IVectorIndex.h
    src/L2/FaissFlatIpIndex.h
    src/L2/MetadataStore.h
    src/L2/PHashIndex.h
    src/L3/IDetector.h
    src/L3/IEmbeddingExtractor.h
    src/L3/ScrfdFaceDetector.h
    src/L3/ArcFaceExtractor.h
    src/L3/FaceAlign.h
    src/L3/PhotoAuthenticityChecker.h
    src/L3/PHasher.h
    src/L3/OrbCropMatcher.h
    src/L3/IPoseDetector.h
    src/L3/YoloPoseDetector.h
    src/L3/DinoV2Extractor.h
    src/L4/FaceRecognitionPipeline.h
    src/L4/ImageMatchPipeline.h
    src/L4/ClothingRecognitionPipeline.h
    src/L5/ResultFusion.h
    src/L5/RetrievalOrchestrator.h
)

add_library(CoserRetrievalCore STATIC ${CORE_SOURCES} ${CORE_HEADERS})

target_compile_definitions(CoserRetrievalCore PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

target_compile_features(CoserRetrievalCore PRIVATE cxx_std_17)

if(MSVC)
    target_compile_options(CoserRetrievalCore PRIVATE /utf-8 /W3 /wd4996)
endif()

target_include_directories(CoserRetrievalCore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# ─── 引入三方库（对核心库生效，PROJECT_NAME 临时指向 CoserRetrievalCore） ──────
set(PROJECT_NAME CoserRetrievalCore)

include(${ProjectRootDir}/ThirdParty/cmake/Add3rd_sqlite3.cmake)
include(${ProjectRootDir}/ThirdParty/cmake/Add3rd_nlohmann_json.cmake)
include(${ProjectRootDir}/ThirdParty/cmake/Add3rd_faiss.cmake)
include(${ProjectRootDir}/ThirdParty/cmake/Add3rd_opencv4.cmake)
include(${ProjectRootDir}/ThirdParty/cmake/Add3rd_onnxruntime.cmake)

set(PROJECT_NAME CoserRetrieval)

# ─── CLI 可执行程序 ───────────────────────────────────────────────────────────
add_executable(ingest_cli apps/ingest_cli.cpp)
add_executable(query_cli apps/query_cli.cpp)

foreach(_exe ingest_cli query_cli)
    target_compile_features(${_exe} PRIVATE cxx_std_17)
    target_compile_definitions(${_exe} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    if(MSVC)
        target_compile_options(${_exe} PRIVATE /utf-8 /W3 /wd4996)
    endif()
    target_link_libraries(${_exe} PRIVATE
        CoserRetrievalCore
        opencv_world
        unofficial::sqlite3::sqlite3
        faiss
        onnxruntime::onnxruntime
        onnxruntime::providers_shared
    )
endforeach()

# ─── 安装 ─────────────────────────────────────────────────────────────────────
install(TARGETS ingest_cli query_cli
    RUNTIME DESTINATION bin
)

# 安装运行时 DLL 依赖（faiss/opencv4/onnxruntime 等）
Install_Imported_Locations()

# faiss 的间接依赖 DLL（openblas/lapack 及 MinGW 运行时，未被 Add_Imported_Location 捕获）
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

# ─── 输出信息 ─────────────────────────────────────────────────────────────────
message(STATUS "=== CoserRetrieval Configuration ===")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ standard: ${CMAKE_CXX_STANDARD}")
