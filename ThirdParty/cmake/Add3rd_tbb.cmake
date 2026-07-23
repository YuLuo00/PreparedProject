# 使用前需设置变量: PROJECT_NAME
set(TBB_DIR "${ProjectRootDir}/ThirdParty/tbb/installed/x64-windows/share/tbb//")
message("TBB_DIR == ${TBB_DIR}")

find_package(TBB CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE TBB::tbb)

Add_Interface_Imported_Location(TBB::tbb)
Add_Imported_Location(TBB::tbb)
