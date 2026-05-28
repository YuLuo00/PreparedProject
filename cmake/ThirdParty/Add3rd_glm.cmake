# 使用前需设置变量: ProjectName
# 示例:
#   set(ProjectName MyTarget)
#   include(${ProjectRootDir}/cmake/ThirdParty/glm.cmake)

set(glm_DIR "${ProjectRootDir}/ThirdParty/GLM/installed/x64-windows/share/glm")
message("glm_DIR == ${glm_DIR}")

find_package(glm CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE glm::glm)

Add_Interface_Imported_Location(glm::glm)
