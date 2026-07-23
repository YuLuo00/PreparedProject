# 使用前需设置变量: PROJECT_NAME
set(fmt_DIR "${ProjectRootDir}/ThirdParty/spdlog/installed/x64-windows/share/fmt")
set(spdlog_DIR "${ProjectRootDir}/ThirdParty/spdlog/installed/x64-windows/share/spdlog")
message("spdlog_DIR == ${spdlog_DIR}")

find_package(spdlog CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE spdlog::spdlog)

Add_Interface_Imported_Location(spdlog::spdlog fmt::fmt)
Add_Imported_Location(spdlog::spdlog)
Add_Imported_Location(fmt::fmt)
