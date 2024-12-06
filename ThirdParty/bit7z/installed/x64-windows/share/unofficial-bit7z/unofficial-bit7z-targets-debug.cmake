#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "unofficial::bit7z::bit7z64" for configuration "Debug"
set_property(TARGET unofficial::bit7z::bit7z64 APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(unofficial::bit7z::bit7z64 PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "CXX"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/debug/lib/bit7z64_d.lib"
  )

list(APPEND _cmake_import_check_targets unofficial::bit7z::bit7z64 )
list(APPEND _cmake_import_check_files_for_unofficial::bit7z::bit7z64 "${_IMPORT_PREFIX}/debug/lib/bit7z64_d.lib" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
