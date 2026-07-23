# 使用前需设置变量: PROJECT_NAME
set(CURL_DIR "${ProjectRootDir}/ThirdParty/curl/installed/x64-windows/share/curl/")
set(OpenSSL_DIR "${ProjectRootDir}/ThirdParty/curl/installed/x64-windows/share/openssl/")
set(OPENSSL_ROOT_DIR "${ProjectRootDir}/ThirdParty/curl/installed/x64-windows/")
message("CURL_DIR == ${CURL_DIR}")
set(ZLIB_INCLUDE_DIR "${ProjectRootDir}/ThirdParty/curl/installed/x64-windows/include")
set(ZLIB_LIBRARY_DEBUG "${ProjectRootDir}/ThirdParty/curl/installed/x64-windows/debug/lib/zlibd.lib")
set(ZLIB_LIBRARY_RELEASE "${ProjectRootDir}/ThirdParty/curl/installed/x64-windows/lib/zlib.lib")

find_package(CURL CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE CURL::libcurl)

file(GLOB CURL_RUNTIME_DLLS_DEBUG
    "${ProjectRootDir}/ThirdParty/curl/installed/x64-windows/debug/bin/*.dll"
)
file(GLOB CURL_RUNTIME_DLLS_RELEASE
    "${ProjectRootDir}/ThirdParty/curl/installed/x64-windows/bin/*.dll"
)

Add_Interface_Imported_Location(CURL::libcurl)
Add_Imported_Location(CURL::libcurl)
list(APPEND ALL_IMPORTED_LOCATION_Debug ${CURL_RUNTIME_DLLS_DEBUG})
list(APPEND ALL_IMPORTED_LOCATION_Release ${CURL_RUNTIME_DLLS_RELEASE})
