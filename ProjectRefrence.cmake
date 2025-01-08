# 项目基准路径
set(ProjectRootDir ${CMAKE_CURRENT_LIST_DIR})
message("ProjectRootDir == ${ProjectRootDir}")
set(ALL_IMPORTED_LOCATION "")
set(ALL_IMPORTED_LOCATION_Debug "")
set(ALL_IMPORTED_LOCATION_Release "")

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)

# 设置解决方案的输出路径
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${ProjectRootDir}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${ProjectRootDir}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${ProjectRootDir}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${ProjectRootDir}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${ProjectRootDir}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${ProjectRootDir}/bin)

if(NOT DEFINED INITIALED)
    set(INITIALED "" CACHE STRING "initial flag" FORCE)
    message(--------INITIAL_SOME_CONFIGURATIONS----------)
    set(CMAKE_CONFIGURATION_TYPES "Release" CACHE STRING "Limited build configurations" FORCE)
endif()

# 添加分组
macro(target_sources_group TargetName GroupName PERMISSION)
    target_sources(${TargetName} ${PERMISSION} ${ARGN})
    source_group(${GroupName} FILES ${ARGN})
endmacro()

# 添加运行时依赖文件
# 添加运行时依赖文件
macro(Add_Interface_Imported_Location ProjectNames)
    foreach(ProjectName ${ARGV})
    # foreach(ProjectName ${ProjectNames})
        message("add dependency dlls for ${ProjectName}")
        get_target_property(_locs ${ProjectName} INTERFACE_IMPORTED_LOCATION)
        if(NOT _locs STREQUAL "_locs-NOTFOUND")
            message(STATUS "Adding INTERFACE_IMPORTED_LOCATION for ${ProjectName}")
            list(APPEND ALL_IMPORTED_LOCATION ${_locs})
        endif()

        get_target_property(_locs ${ProjectName} INTERFACE_IMPORTED_LOCATION_DEBUG)
        if(NOT _locs STREQUAL "_locs-NOTFOUND")
            message(STATUS "Adding INTERFACE_IMPORTED_LOCATION_DEBUG for ${ProjectName}")
            list(APPEND ALL_IMPORTED_LOCATION_Debug ${_locs})
        endif()

        get_target_property(_locs ${ProjectName} IMPORTED_LOCATION_DEBUG)
        if(NOT _locs STREQUAL "_locs-NOTFOUND")
            message(STATUS "Adding IMPORTED_LOCATION_DEBUG for ${ProjectName}")
            list(APPEND ALL_IMPORTED_LOCATION_Debug ${_locs})
        endif()
        

        get_target_property(_locs ${ProjectName} INTERFACE_IMPORTED_LOCATION_RELEASE)
        if(NOT _locs STREQUAL "_locs-NOTFOUND")
            message(STATUS "Adding INTERFACE_IMPORTED_LOCATION_RELEASE for ${ProjectName}")
            list(APPEND ALL_IMPORTED_LOCATION_Release ${_locs})
        endif()

        get_target_property(_locs ${ProjectName} IMPORTED_LOCATION_RELEASE)
        if(NOT _locs STREQUAL "_locs-NOTFOUND")
            message(STATUS "Adding IMPORTED_LOCATION_RELEASE for ${ProjectName}")
            list(APPEND ALL_IMPORTED_LOCATION_Release ${_locs})
        endif()
    endforeach()
endmacro()


# -------------------------------------------------------------------------------- 导入三方库 -------------------------------------

# macro(Add3rd_ ProjectName)
# endmacro()
# endmacro()
macro(Add3rd_ ProjectName)
    set(_DIR "${ProjectRootDir}/ThirdParty/")
    message("_DIR == ${_DIR}")
    # this is heuristically generated, and may not be correct
    find_package( CONFIG REQUIRED)
    target_link_libraries(${ProjectName} PRIVATE )

    Add_Interface_Imported_Location()
endmacro()

macro(Add3rd_libtorch ProjectName)
    # 检查是否为多配置生成器
    if(CMAKE_CONFIGURATION_TYPES)
        # 获取配置项的数量
        list(LENGTH CMAKE_CONFIGURATION_TYPES CONFIG_COUNT)
        if(NOT CONFIG_COUNT EQUAL 1)
            message(FATAL_ERROR "CMAKE_CONFIGURATION_TYPES must contain only one configuration. Current configurations: ${CMAKE_CONFIGURATION_TYPES}.")
        endif()
    endif()
    
    if(CMAKE_CONFIGURATION_TYPES)
        message("Multiple Configurations == [${CMAKE_CONFIGURATION_TYPES}]")
        set(CONFIG_SINGLE ${CMAKE_CONFIGURATION_TYPES})
    else()
        message("Single Configurations == [${CMAKE_BUILD_TYPE}]")
        set(CONFIG_SINGLE ${CMAKE_BUILD_TYPE})
    endif()
    # 设置 Torch_DIR
    if(CONFIG_SINGLE STREQUAL "Debug")
        set(Torch_DIR "${ProjectRootDir}/ThirdParty/libtorch-win-shared-with-deps-2.5.1+cpu/libtorch/share/cmake/Torch/")
    elseif(CONFIG_SINGLE STREQUAL "Release")
        set(Torch_DIR "${ProjectRootDir}/ThirdParty/libtorch-win-shared-with-deps-debug-2.5.1+cpu/libtorch/share/cmake/Torch/")
    else()
        message(FATAL_ERROR "[Add3rd_libtorch] Unsupported build type. Please specify Debug or Release. [${CONFIG_SINGLE}]")
    endif()

    message("torch_DIR == ${Torch_DIR}")
    # this is heuristically generated, and may not be correct
    find_package(Torch CONFIG REQUIRED)
    target_link_libraries(${ProjectName} PRIVATE torch)

    Add_Interface_Imported_Location(torch)
endmacro()

macro(Add3rd_CURL ProjectName)
    set(CURL_DIR "${ProjectRootDir}/ThirdParty/curl/installed/x64-windows/share/curl/")
    message("CURL_DIR == ${CURL_DIR}")
    # this is heuristically generated, and may not be correct
    find_package(CURL CONFIG REQUIRED)
    target_link_libraries(${ProjectName} PRIVATE CURL::libcurl CURL::libcurl_shared)

    Add_Interface_Imported_Location(CURL::libcurl CURL::libcurl_shared)
    list(APPEND ALL_IMPORTED_LOCATION_Debug)
    
    file(GLOB_RECURSE DLL_FILES "${ProjectRootDir}/ThirdParty/curl/installed/x64-windows/bin/*.dll")
    foreach(_dll ${DLL_FILES})
        message(STATUS " ${_dll}")
        list(APPEND ALL_IMPORTED_LOCATION_Release ${_dll})
    endforeach()
    file(GLOB_RECURSE DLL_FILES "${ProjectRootDir}/ThirdParty/curl/installed/x64-windows/debug/bin/*.dll")
    foreach(_dll ${DLL_FILES})
        message(STATUS " ${_dll}")
        list(APPEND ALL_IMPORTED_LOCATION_Debug ${_dll})
    endforeach()
endmacro()

macro(Add3rd_tinygltf ProjectName)
    target_include_directories(${ProjectName} PRIVATE
        "${ProjectRootDir}/ThirdParty/tinygltf/installed/x64-windows/include/"
    )
endmacro()

macro(Add3rd_OpenCV ProjectName)
    set(Protobuf_DIR "${ProjectRootDir}/ThirdParty/opencv/installed/x64-windows/share/protobuf/")
    set(quirc_DIR "${ProjectRootDir}/ThirdParty/opencv/installed/x64-windows/share/quirc/")
    set(OpenCV_DIR "${ProjectRootDir}/ThirdParty/opencv/installed/x64-windows/share/opencv4/")
    
    set(TIFF_INCLUDE_DIR "${ProjectRootDir}/ThirdParty/opencv/installed/x64-windows/include/")
    set(TIFF_LIBRARY "${ProjectRootDir}/ThirdParty/opencv/installed/x64-windows/bin/")
    list(APPEND ALL_IMPORTED_LOCATION_Debug "${ProjectRootDir}/ThirdParty/opencv/installed/x64-windows/debug/bin/tiffd.dll")
    list(APPEND ALL_IMPORTED_LOCATION_Release "${ProjectRootDir}/ThirdParty/opencv/installed/x64-windows/bin/tiff.dll")
    message("OpenCV_DIR == ${OpenCV_DIR}")

    set(libjpeg-turbo_DIR "${ProjectRootDir}/ThirdParty/opencv/installed/x64-windows/share/libjpeg-turbo/")
    set(WebP_DIR "${ProjectRootDir}/ThirdParty/opencv/installed/x64-windows/share/WebP/")
    set(libpng_DIR "${ProjectRootDir}/ThirdParty/opencv/installed/x64-windows/share/libpng/")

    set(ZLIB_LIBRARY ${ProjectRootDir}ThirdParty/opencv/installed/x64-windows/bin/)
    set(ZLIB_INCLUDE_DIR ${ProjectRootDir}ThirdParty/opencv/installed/x64-windows/include/)
    find_package(libpng CONFIG REQUIRED)
    find_package(libjpeg-turbo CONFIG REQUIRED)
    find_package(WebP CONFIG REQUIRED)
    # this is heuristically generated, and may not be correct
    find_package(OpenCV CONFIG REQUIRED)
    

    # note: 10 additional targets are not displayed.
    target_link_libraries(${ProjectName} PRIVATE
        opencv_ml opencv_dnn opencv_core opencv_flann opencv_imgcodecs opencv_imgproc
        libjpeg-turbo::jpeg
        WebP::webp WebP::webpdecoder WebP::webpdemux WebP::sharpyuv
        png
    )
    Add_Interface_Imported_Location(
        opencv_ml opencv_dnn opencv_core opencv_flann opencv_imgcodecs opencv_imgproc
        libjpeg-turbo::jpeg
        WebP::webp WebP::webpdecoder WebP::webpdemux WebP::sharpyuv
        png
    )
endmacro()

macro(Add3rd_fmt ProjectName)
    set(fmt_DIR "${ProjectRootDir}/ThirdParty/fmt/installed/x64-windows/share/fmt/")
    message("fmt_DIR == ${fmt_DIR}")
    # this is heuristically generated, and may not be correct
    find_package(fmt CONFIG REQUIRED)
    target_link_libraries(${ProjectName} PRIVATE fmt::fmt-header-only)

    Add_Interface_Imported_Location(fmt::fmt-header-only)
endmacro()

macro(Add3rd_nlohmann_json ProjectName)
    set(nlohmann_json_DIR "${ProjectRootDir}/ThirdParty/nlohmann/installed/x64-windows/share/nlohmann_json/")
    message("nlohmann_json_DIR == ${nlohmann_json_DIR}")
    # this is heuristically generated, and may not be correct
    find_package(nlohmann_json CONFIG REQUIRED)
    target_link_libraries(${ProjectName} PRIVATE nlohmann_json::nlohmann_json)
    Add_Interface_Imported_Location(nlohmann_json::nlohmann_json)
endmacro()

macro(Add3rd_LibArchive ProjectName)
    message("LibArchive == ${ProjectRootDir}/ThirdParty/libarchive/installed/x64-windows/")
    set(CMAKE_INCLUDE_PATH "${ProjectRootDir}/ThirdParty/libarchive/installed/x64-windows/include")
    set(CMAKE_LIBRARY_PATH "${ProjectRootDir}/ThirdParty/libarchive/installed/x64-windows/lib")

    find_package(LibArchive REQUIRED)
    target_link_libraries(${ProjectName} PRIVATE LibArchive::LibArchive) # since CMake 3.17

    install(DIRECTORY
        $<$<CONFIG:Debug>:${ProjectRootDir}/ThirdParty/libarchive/installed/x64-windows/debug/bin/>
        $<$<CONFIG:Release>:${ProjectRootDir}/ThirdParty/libarchive/installed/x64-windows/bin/>
        DESTINATION bin
        FILES_MATCHING PATTERN "*.dll"
    )
endmacro()

macro(Add3rd_7zExtra ProjectName)
    list(APPEND ALL_IMPORTED_LOCATION "${ProjectRootDir}/ThirdParty/7z2409-extra/x64/7za.dll")
endmacro()

macro(Add3rd_bit7z ProjectName)
    set(ghc_filesystem_DIR "${ProjectRootDir}/ThirdParty/bit7z/installed/x64-windows/share/ghc_filesystem/")
    set(7zip_DIR "${ProjectRootDir}/ThirdParty/bit7z/installed/x64-windows/share/7zip/")
    set(unofficial-bit7z_DIR "${ProjectRootDir}/ThirdParty/bit7z/installed/x64-windows/share/unofficial-bit7z/")
    message("unofficial-bit7z_DIR == ${unofficial-bit7z_DIR}")
    # this is heuristically generated, and may not be correct
    find_package(unofficial-bit7z CONFIG REQUIRED)
    target_link_libraries(${ProjectName} PRIVATE unofficial::bit7z::bit7z64)

    Add_Interface_Imported_Location(unofficial::bit7z::bit7z64)
    Add_Interface_Imported_Location(7zip::7zip)
endmacro()

macro(Add3rd_vulkan ProjectName)
    set(vulkan_DIR "${ProjectRootDir}/ThirdParty/vulkan/installed/x64-windows/share/VulkanLoader/")
    message("vulkan_DIR == ${vulkan_DIR}")
    # https://cmake.org/cmake/help/latest/module/FindVulkan.html
    find_package(Vulkan REQUIRED)
    target_link_libraries(${ProjectName} PRIVATE Vulkan::Vulkan)

    Add_Interface_Imported_Location(Vulkan::Vulkan)
endmacro()

macro(Add3rd_stb_image ProjectName)
    target_include_directories(${ProjectName} PRIVATE
        "${ProjectRootDir}/ThirdParty/stb_image/"
    )
endmacro()

macro(Add3rd_freeimage ProjectName)
    set(libpng_DIR "${ProjectRootDir}/ThirdParty/libpng/installed/x64-windows/share/libpng/")
    find_package(PNG REQUIRED)
    target_link_libraries(${ProjectName} PRIVATE PNG::PNG)
    Add_Interface_Imported_Location(PNG::PNG)

    set(freeimage_DIR "${ProjectRootDir}/ThirdParty/freeimage/installed/x64-windows/share/freeimage/")
    message("freeimage_DIR == ${freeimage_DIR}")
    # this is heuristically generated, and may not be correct
    find_package(freeimage CONFIG REQUIRED)
    target_link_libraries(${ProjectName} PRIVATE
        freeimage::FreeImage
        # freeimage::FreeImagePlus
    )

    Add_Interface_Imported_Location(freeimage::FreeImage)
    # Add_Interface_Imported_Location(freeimage::FreeImagePlus)
endmacro()

macro(Add3rd_glm ProjectName)
    set(glm_DIR "${ProjectRootDir}/ThirdParty/GLM/installed/x64-windows/share/glm")
    message("glm_DIR == ${glm_DIR}")
    # this is heuristically generated, and may not be correct
    find_package(glm CONFIG REQUIRED)
    target_link_libraries(${ProjectName} PRIVATE glm::glm)

    Add_Interface_Imported_Location(glm::glm)
endmacro()

macro(Add3rd_Glad ProjectName)
    set(glad_DIR "${ProjectRootDir}/ThirdParty/glad/installed/x64-windows/share/glad")
    message("glad_DIR == ${glad_DIR}")
    # this is heuristically generated, and may not be correct
    find_package(glad CONFIG REQUIRED)
    target_link_libraries(${ProjectName} PRIVATE glad::glad)
    Add_Interface_Imported_Location(glad::glad)
endmacro()

macro(Add3rd_glfw3 ProjectName)
    set(glfw3_DIR "${ProjectRootDir}/ThirdParty/GLFW3/installed/x64-windows/share/glfw3")
    message("glfw3_DIR == ${glfw3_DIR}")
    # this is heuristically generated, and may not be correct
    find_package(glfw3 CONFIG REQUIRED)
    target_link_libraries(${ProjectName} PRIVATE glfw)

    Add_Interface_Imported_Location(glfw)
endmacro()


macro(Add3rd_GLEW ProjectName)
    set(GLEW_DIR "${ProjectRootDir}/ThirdParty/GLEW/installed/x64-windows/share/glew")
    message("GLEW_DIR == ${GLEW_DIR}")
    # this is heuristically generated, and may not be correct
    find_package(GLEW REQUIRED)
    target_link_libraries(${ProjectName} PRIVATE GLEW::GLEW)

    Add_Interface_Imported_Location(GLEW::GLEW)
endmacro()