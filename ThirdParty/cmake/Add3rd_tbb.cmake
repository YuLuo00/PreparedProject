if(MINGW)
    set(TBB_DIR "${ProjectRootDir}/ThirdParty/install/tbb/installed/x64-mingw-dynamic/lib/cmake/TBB")
else()
    set(TBB_DIR "${ProjectRootDir}/ThirdParty/install/tbb/installed/x64-windows/share/tbb")
endif()
message("TBB_DIR == ${TBB_DIR}")

find_package(TBB CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE TBB::tbb)

Add_Interface_Imported_Location(TBB::tbb)
Add_Imported_Location(TBB::tbb)
