# 使用前需设置变量: ProjectName
# 示例:
#   set(ProjectName MyTarget)
#   include(${ProjectRootDir}/ThirdParty/cmake/Add3rd_sqlite3.cmake)

set(unofficial-sqlite3_DIR "${ProjectRootDir}/ThirdParty/install/sqlite3/installed/x64-windows/share/unofficial-sqlite3/")
message("unofficial-sqlite3_DIR == ${unofficial-sqlite3_DIR}")

find_package(unofficial-sqlite3 CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE unofficial::sqlite3::sqlite3)

Add_Interface_Imported_Location(unofficial::sqlite3::sqlite3)
Add_Imported_Location(unofficial::sqlite3::sqlite3)
