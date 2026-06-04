# 使用前需设置变量: ProjectName
# 示例:
#   set(ProjectName MyTarget)
#   include(${ProjectRootDir}/ThirdParty/cmake/Add3rd_faiss.cmake)

set(_faiss_root "${ProjectRootDir}/ThirdParty/install/faiss/installed/x64-windows")

set(faiss_DIR    "${_faiss_root}/share/faiss/"           CACHE PATH "" FORCE)
set(OpenBLAS_DIR "${_faiss_root}/share/openblas/"        CACHE PATH "" FORCE)
set(openblas_DIR "${_faiss_root}/share/openblas/"        CACHE PATH "" FORCE)
set(lapack_DIR   "${_faiss_root}/share/lapack-reference/" CACHE PATH "" FORCE)

message("faiss_DIR == ${faiss_DIR}")

# 先找 OpenBLAS（CONFIG 模式），再预设 BLAS_FOUND/BLAS_LIBRARIES，
# 使 lapack-config.cmake 内部的 find_dependency(BLAS) 直接通过
find_package(OpenBLAS CONFIG REQUIRED)
set(BLAS_FOUND     TRUE                              CACHE BOOL   "" FORCE)
set(BLAS_LIBRARIES "${_faiss_root}/lib/openblas.lib" CACHE STRING "" FORCE)

find_package(lapack CONFIG REQUIRED)
find_package(faiss  CONFIG REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE faiss lapack)

Add_Interface_Imported_Location(faiss lapack)
Add_Imported_Location(faiss lapack)
