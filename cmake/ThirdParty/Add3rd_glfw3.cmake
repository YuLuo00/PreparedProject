# 使用前需设置变量: ProjectName
# 示例:
#   set(ProjectName MyTarget)
#   include(${ProjectRootDir}/cmake/ThirdParty/glfw3.cmake)

set(glfw3_DIR "${ProjectRootDir}/ThirdParty/GLFW3/installed/x64-windows/share/glfw3")
message("glfw3_DIR == ${glfw3_DIR}")

find_package(glfw3 CONFIG REQUIRED)
target_link_libraries(${ProjectName} PRIVATE glfw)

Add_Interface_Imported_Location(glfw)
