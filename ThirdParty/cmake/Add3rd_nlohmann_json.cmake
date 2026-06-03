# 使用前需设置变量: ProjectName
# 示例:
#   set(ProjectName MyTarget)
#   include(${ProjectRootDir}/cmake/ThirdParty/nlohmann_json.cmake)

set(nlohmann_json_DIR "${ProjectRootDir}/ThirdParty/nlohmann/installed/x64-windows/share/nlohmann_json/")
message("nlohmann_json_DIR == ${nlohmann_json_DIR}")

find_package(nlohmann_json CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE nlohmann_json::nlohmann_json)

Add_Interface_Imported_Location(nlohmann_json::nlohmann_json)
