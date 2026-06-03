# 使用前需设置变量: ProjectName
# 示例:
#   set(ProjectName MyTarget)
#   include(${ProjectRootDir}/cmake/ThirdParty/opencv411.cmake)

set(Protobuf_DIR "${ProjectRootDir}/ThirdParty/opencv411/installed/x64-windows/share/protobuf/")
set(quirc_DIR "${ProjectRootDir}/ThirdParty/opencv411/installed/x64-windows/share/quirc/")
set(OpenCV_DIR "${ProjectRootDir}/ThirdParty/opencv411/installed/x64-windows/share/opencv4/")

set(TIFF_INCLUDE_DIR "${ProjectRootDir}/ThirdParty/opencv411/installed/x64-windows/include/")
set(TIFF_LIBRARY "${ProjectRootDir}/ThirdParty/opencv411/installed/x64-windows/bin/")
list(APPEND ALL_IMPORTED_LOCATION_Debug "${ProjectRootDir}/ThirdParty/opencv411/installed/x64-windows/debug/bin/tiffd.dll")
list(APPEND ALL_IMPORTED_LOCATION_Release "${ProjectRootDir}/ThirdParty/opencv411/installed/x64-windows/bin/tiffd.dll")
message("OpenCV_DIR == ${OpenCV_DIR}")

set(libjpeg-turbo_DIR "${ProjectRootDir}/ThirdParty/opencv411/installed/x64-windows/share/libjpeg-turbo/")
set(WebP_DIR "${ProjectRootDir}/ThirdParty/opencv411/installed/x64-windows/share/WebP/")
set(libpng_DIR "${ProjectRootDir}/ThirdParty/opencv411/installed/x64-windows/share/libpng/")
find_package(libpng CONFIG REQUIRED)
find_package(libjpeg-turbo CONFIG REQUIRED)
find_package(WebP CONFIG REQUIRED)
find_package(OpenCV CONFIG REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE
    opencv_ml opencv_dnn opencv_core opencv_flann opencv_imgcodecs opencv_imgproc opencv_highgui
    libjpeg-turbo::jpeg
    WebP::webp WebP::webpdecoder WebP::webpdemux WebP::sharpyuv WebP::libwebpmux
    png
)
Add_Interface_Imported_Location(
    opencv_ml opencv_dnn opencv_core opencv_flann opencv_imgcodecs opencv_imgproc opencv_highgui
    libjpeg-turbo::jpeg
    WebP::webp WebP::webpdecoder WebP::webpdemux WebP::sharpyuv WebP::libwebpmux
    png
)
