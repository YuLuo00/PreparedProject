cmake_minimum_required(VERSION 3.8)

include("${CMAKE_CURRENT_LIST_DIR}/ProjectRefrence.cmake")

project(RootProject)
set(CMAKE_CXX_STANDARD 17)
message("${PROJECT_NAME} --------------------- ffffffff")

# 添加项目：导入c#的窗体程序
add_custom_target(ZERO_CHECK_with_cSharp ALL
    COMMAND dotnet sln "${CMAKE_BINARY_DIR}/${PROJECT_NAME}.sln"
                add "${ProjectRootDir}/RawMoveUI/RawMoveUI.csproj"
    WORKING_DIRECTORY "${ProjectRootDir}/bin"
    COMMENT "Ensure C# project is added to solution"
)
set_property(TARGET ZERO_CHECK_with_cSharp PROPERTY FOLDER "CMakePredefinedTargets")

add_subdirectory("${ProjectRootDir}/RawMove/")
# add_dependencies(RawMove ZERO_CHECK_with_cSharp)

# add_subdirectory("${ProjectRootDir}/RawMoveCLR/")
# add_dependencies(RawMoveCLR ZERO_CHECK_with_cSharp)
# add_dependencies(RawMoveCLR RawMove)
# cmake_policy(SET CMP0079 NEW)
# target_link_libraries(RawMoveCLR RawMove)
