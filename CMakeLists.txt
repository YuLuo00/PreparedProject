cmake_minimum_required(VERSION 3.13) # CMP0079 introduced in CMake 3.13
cmake_policy(SET CMP0079 NEW)


include("${CMAKE_CURRENT_LIST_DIR}/ProjectRefrence.cmake")

project(Test_build_Cpp_linux)
set(CMAKE_CXX_STANDARD 17)
message("${PROJECT_NAME} --------------------- ffffffff")

add_subdirectory("${ProjectRootDir}/ExeModule/")
add_subdirectory("${ProjectRootDir}/DllModule/")

target_link_libraries(ExeModule PRIVATE DllModule)

add_dependencies(ExeModule DllModule)


