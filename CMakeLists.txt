cmake_minimum_required(VERSION 3.8)

include("${CMAKE_CURRENT_LIST_DIR}/ProjectRefrence.cmake")

project(RootProject)
set(CMAKE_CXX_STANDARD 17)
message("${PROJECT_NAME} --------------------- ffffffff")

add_subdirectory("${ProjectRootDir}/Test/")
