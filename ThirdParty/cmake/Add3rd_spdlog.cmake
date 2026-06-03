if(MINGW)
    set(fmt_DIR "${ProjectRootDir}/ThirdParty/spdlog/installed/x64-mingw-dynamic/share/fmt")
    set(spdlog_DIR "${ProjectRootDir}/ThirdParty/spdlog/installed/x64-mingw-dynamic/share/spdlog")
else()
    set(fmt_DIR "${ProjectRootDir}/ThirdParty/spdlog/installed/x64-windows/share/fmt")
    set(spdlog_DIR "${ProjectRootDir}/ThirdParty/spdlog/installed/x64-windows/share/spdlog")
endif()
message("spdlog_DIR == ${spdlog_DIR}")

find_package(spdlog CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE spdlog::spdlog)

Add_Interface_Imported_Location(spdlog::spdlog)
