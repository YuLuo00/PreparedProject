if(MINGW)
    set(fmt_DIR "${ProjectRootDir}/ThirdParty/fmt/installed/x64-mingw-dynamic/share/fmt/")
else()
    set(fmt_DIR "${ProjectRootDir}/ThirdParty/fmt/installed/x64-windows/share/fmt/")
endif()
message("fmt_DIR == ${fmt_DIR}")

find_package(fmt CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE fmt::fmt)

Add_Interface_Imported_Location(fmt::fmt)
