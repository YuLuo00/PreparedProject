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

# 设置仅生成 Debug 和 Release 配置
set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "Limited build configurations" FORCE)

# 添加分组
macro(target_sources_group TargetName GroupName PERMISSION)
    target_sources(${TargetName} ${PERMISSION} ${ARGN})
    source_group(${GroupName} FILES ${ARGN})
endmacro()

# 添加运行时依赖文件
macro(Add_Interface_Imported_Location ProjectName)
    # get_target_property(_locs ${ProjectName} INTERFACE_IMPORTED_LOCATION)
    # if(NOT _locs STREQUAL "_locs-NOTFOUND")
    #     message(STATUS "MY_VAR is _locs-NOTFOUND")
    #     list(APPEND ALL_IMPORTED_LOCATION ${_locs})
    # endif()
    
    get_target_property(_locs ${ProjectName} INTERFACE_IMPORTED_LOCATION_DEBUG)
    if(NOT _locs STREQUAL "_locs-NOTFOUND")
        message(STATUS "MY_VAR is _locs-NOTFOUND")
        list(APPEND ALL_IMPORTED_LOCATION_Debug ${_locs})
    endif()

    get_target_property(_locs ${ProjectName} INTERFACE_IMPORTED_LOCATION_RELEASE)
    if(NOT _locs STREQUAL "_locs-NOTFOUND")
        message(STATUS "MY_VAR is _locs-NOTFOUND")
        list(APPEND ALL_IMPORTED_LOCATION_Release ${_locs})
    endif()
endmacro()

# -------------------------------------------------------------------------------- 导入三方库 -------------------------------------

# macro(Add3rd_ ProjectName)
# endmacro()
macro(Add3rd_ ProjectName)
    set(_DIR "${ProjectRootDir}/ThirdParty/")
    message("_DIR == ${_DIR}")
    # this is heuristically generated, and may not be correct
    find_package( CONFIG REQUIRED)
    target_link_libraries(${ProjectName} PRIVATE )

    Add_Interface_Imported_Location()
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
    # set(LibArchive_DIR "${ProjectRootDir}/ThirdParty/libarchive/installed/x64-windows")
    # message("LibArchive_DIR == ${LibArchive_DIR}")
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

    # file(GLOB _DLL_Release "*.dll")
    # foreach(_dll ${_DLL_Release})
    #     list(APPEND ALL_IMPORTED_LOCATION_Release ${_dll})
    #     message(STATUS "Found DLL: ${_dll}")
    # endforeach()

    # file(GLOB _DLL_Debug )
    # foreach(_dll ${_DLL_Debug})
    #     list(APPEND ALL_IMPORTED_LOCATION_Debug ${_dll})
    #     message(STATUS "Found DLL: ${_dll}")
    # endforeach()
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
    target_compile_definitions(${ProjectName} PRIVATE
        STB_IMAGE_IMPLEMENTATION
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