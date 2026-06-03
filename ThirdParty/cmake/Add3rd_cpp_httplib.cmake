# 使用前需设置变量: ProjectName
# 示例:
#   set(ProjectName MyTarget)
#   include(${ProjectRootDir}/ThirdParty/cmake/Add3rd_cpp_httplib.cmake)

set(httplib_DIR "${ProjectRootDir}/ThirdParty/install/cpp-httplib/installed/x64-windows/share/httplib/")
message("httplib_DIR == ${httplib_DIR}")

# brotli 依赖
set(unofficial-brotli_DIR "${ProjectRootDir}/ThirdParty/install/cpp-httplib/installed/x64-windows/share/unofficial-brotli/")
set(brotli_DIR "${ProjectRootDir}/ThirdParty/install/cpp-httplib/installed/x64-windows/share/brotli/")

find_package(httplib CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE httplib::httplib)

Add_Interface_Imported_Location(httplib::httplib)
