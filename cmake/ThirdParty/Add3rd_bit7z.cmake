# 使用前需设置变量: ProjectName
# 示例:
#   set(ProjectName MyTarget)
#   include(${ProjectRootDir}/cmake/ThirdParty/bit7z.cmake)

set(ghc_filesystem_DIR "${ProjectRootDir}/ThirdParty/bit7z/installed/x64-windows/share/ghc_filesystem/")
set(7zip_DIR "${ProjectRootDir}/ThirdParty/bit7z/installed/x64-windows/share/7zip/")
set(unofficial-bit7z_DIR "${ProjectRootDir}/ThirdParty/bit7z/installed/x64-windows/share/unofficial-bit7z/")
message("unofficial-bit7z_DIR == ${unofficial-bit7z_DIR}")

find_package(unofficial-bit7z CONFIG REQUIRED)
target_link_libraries(${ProjectName} PRIVATE unofficial::bit7z::bit7z64)

Add_Interface_Imported_Location(unofficial::bit7z::bit7z64)
Add_Interface_Imported_Location(7zip::7zip)
