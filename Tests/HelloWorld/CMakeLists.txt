project(HelloWorld)
include(..\\..\\ProjectRefrence.cmake)

add_executable(HelloWorld main.cpp)

set(ProjectName ${PROJECT_NAME})
include(${ProjectRootDir}/cmake/ThirdParty/Add3rd_spdlog.cmake)