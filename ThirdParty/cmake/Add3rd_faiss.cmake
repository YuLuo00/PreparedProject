# 使用前需设置变量: ProjectName
# 示例:
#   set(ProjectName MyTarget)
#   include(${ProjectRootDir}/ThirdParty/cmake/Add3rd_faiss.cmake)

set(faiss_DIR "${ProjectRootDir}/ThirdParty/install/faiss/installed/x64-windows/share/faiss/")
message("faiss_DIR == ${faiss_DIR}")

# faiss 依赖 OpenBLAS/LAPACK
set(BLAS_DIR "${ProjectRootDir}/ThirdParty/install/faiss/installed/x64-windows/share/openblas/")
set(LAPACK_DIR "${ProjectRootDir}/ThirdParty/install/faiss/installed/x64-windows/share/lapack/")
set(openblas_DIR "${ProjectRootDir}/ThirdParty/install/faiss/installed/x64-windows/share/openblas/")
set(lapack_DIR "${ProjectRootDir}/ThirdParty/install/faiss/installed/x64-windows/share/lapack/")

find_package(faiss CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE faiss)

Add_Interface_Imported_Location(faiss)
Add_Imported_Location(faiss)
