cmake_minimum_required(VERSION 3.13) # CMP0079 introduced in CMake 3.13
cmake_policy(SET CMP0079 NEW)


include("${CMAKE_CURRENT_LIST_DIR}/ProjectRefrence.cmake")

project(GltfViewer)
set(CMAKE_CXX_STANDARD 17)
message("${PROJECT_NAME} --------------------- ffffffff")

add_subdirectory("${ProjectRootDir}/GltfViewerCore/")
add_subdirectory("${ProjectRootDir}/GltfViewerCLR/")
# # 添加C#项目
# add_custom_target(ZERO_CHECK_with_cSharp ALL
#     COMMAND dotnet sln "${CMAKE_BINARY_DIR}/${PROJECT_NAME}.sln"
#                 add "${ProjectRootDir}/ArchiveToolUI/ArchiveToolUI.csproj"
#     WORKING_DIRECTORY "${ProjectRootDir}/bin"
#     COMMENT "Ensure C# project is added to solution"
# )
# set_property(TARGET ZERO_CHECK_with_cSharp PROPERTY FOLDER "CMakePredefinedTargets")

# target_link_libraries(GltfViewerCLR PRIVATE GltfViewerCore)
# target_link_libraries(ArchiveToolCLR PRIVATE ArchiveTool)

# add_dependencies(MAIN_PROJECT ZERO_CHECK_with_cSharp)
# add_dependencies(ArchiveTool ZERO_CHECK_with_cSharp)
# add_dependencies(ArchiveToolCLR ArchiveTool)
# add_dependencies(ArchiveToolCLR ZERO_CHECK_with_cSharp)

