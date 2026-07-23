# 使用前需设置变量: PROJECT_NAME
set(nlohmann_json_DIR "${ProjectRootDir}/ThirdParty/nlohmann/installed/x64-windows/share/nlohmann_json/")
message("nlohmann_json_DIR == ${nlohmann_json_DIR}")

find_package(nlohmann_json CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE nlohmann_json::nlohmann_json)

Add_Interface_Imported_Location(nlohmann_json::nlohmann_json)
