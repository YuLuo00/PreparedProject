# 使用前需设置变量: ProjectName
# 示例:
#   set(ProjectName MyTarget)
#   include(${ProjectRootDir}/ThirdParty/cmake/Add3rd_opencv4.cmake)
#
# 注意: 本文件对接的是 OpenCV 官方预编译包 (opencv-4.11.0-windows.exe 解包后的
# build/ 目录)，而非 vcpkg 构建产物。该包只提供单一的 monolithic
# opencv_world4110(d).lib/.dll，没有 opencv_core/opencv_imgproc 等独立组件库，
# 也不需要单独的 libjpeg-turbo/libpng（编解码支持已内置在 opencv_world 中）。

set(_opencv4_root "${ProjectRootDir}/ThirdParty/install/opencv4/build")

set(OpenCV_DIR "${_opencv4_root}/x64/vc16/lib/" CACHE PATH "" FORCE)

message("OpenCV_DIR == ${OpenCV_DIR}")

find_package(OpenCV CONFIG REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE opencv_world)

Add_Interface_Imported_Location(opencv_world)
Add_Imported_Location(opencv_world)
