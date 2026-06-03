# 使用前需设置变量: ProjectName
# 示例:
#   set(ProjectName MyTarget)
#   include(${ProjectRootDir}/cmake/ThirdParty/glew.cmake)

set(GLEW_DIR "${ProjectRootDir}/ThirdParty/GLEW/installed/x64-windows/share/glew")
message("GLEW_DIR == ${GLEW_DIR}")

find_package(GLEW REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE GLEW::GLEW)

Add_Interface_Imported_Location(GLEW::GLEW)
