# 使用前需设置变量: ProjectName
# 示例:
#   set(ProjectName MyTarget)
#   include(${ProjectRootDir}/cmake/ThirdParty/fmt.cmake)

set(fmt_DIR "${ProjectRootDir}/ThirdParty/fmt/installed/x64-windows/share/fmt/")
message("fmt_DIR == ${fmt_DIR}")

find_package(fmt CONFIG REQUIRED)
target_link_libraries(${ProjectName} PRIVATE fmt::fmt-header-only)

Add_Interface_Imported_Location(fmt::fmt-header-only)
