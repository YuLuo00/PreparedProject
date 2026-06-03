# 使用前需设置变量: ProjectName
# 示例:
#   set(ProjectName MyTarget)
#   include(${ProjectRootDir}/cmake/ThirdParty/glad.cmake)

set(glad_DIR "${ProjectRootDir}/ThirdParty/glad/installed/x64-windows/share/glad")
message("glad_DIR == ${glad_DIR}")

find_package(glad CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE glad::glad)

Add_Interface_Imported_Location(glad::glad)
