project(HelloWorld)
include(..\\..\\ProjectRefrence.cmake)

add_executable(HelloWorld main.cpp)

include(${ProjectRootDir}/cmake/ThirdParty/Add3rd_spdlog.cmake)