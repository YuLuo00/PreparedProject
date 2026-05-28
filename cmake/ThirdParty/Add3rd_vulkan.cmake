# 使用前需设置变量: ProjectName
# 示例:
#   set(ProjectName MyTarget)
#   include(${ProjectRootDir}/cmake/ThirdParty/vulkan.cmake)

set(vulkan_DIR "${ProjectRootDir}/ThirdParty/vulkan/installed/x64-windows/share/VulkanLoader/")
message("vulkan_DIR == ${vulkan_DIR}")

find_package(Vulkan REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan::Vulkan)

Add_Interface_Imported_Location(Vulkan::Vulkan)
