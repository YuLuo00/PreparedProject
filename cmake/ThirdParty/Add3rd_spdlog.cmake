# 使用前需设置变量: ProjectName
# 示例:
#   set(ProjectName MyTarget)
#   include(${ProjectRootDir}/cmake/ThirdParty/spdlog.cmake)

set(fmt_DIR "${ProjectRootDir}/ThirdParty/spdlog/installed/x64-windows/share/fmt")
set(spdlog_DIR "${ProjectRootDir}/ThirdParty/spdlog/installed/x64-windows/share/spdlog")
message("spdlog_DIR == ${spdlog_DIR}")

find_package(spdlog CONFIG REQUIRED)
target_link_libraries(${ProjectName} PRIVATE spdlog::spdlog)

Add_Interface_Imported_Location(spdlog::spdlog)
