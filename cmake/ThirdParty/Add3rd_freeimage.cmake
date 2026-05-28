# 使用前需设置变量: ProjectName
# 示例:
#   set(ProjectName MyTarget)
#   include(${ProjectRootDir}/cmake/ThirdParty/freeimage.cmake)

set(libpng_DIR "${ProjectRootDir}/ThirdParty/libpng/installed/x64-windows/share/libpng/")
find_package(PNG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE PNG::PNG)
Add_Interface_Imported_Location(PNG::PNG)

set(freeimage_DIR "${ProjectRootDir}/ThirdParty/freeimage/installed/x64-windows/share/freeimage/")
message("freeimage_DIR == ${freeimage_DIR}")

find_package(freeimage CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE freeimage::FreeImage)

Add_Interface_Imported_Location(freeimage::FreeImage)
