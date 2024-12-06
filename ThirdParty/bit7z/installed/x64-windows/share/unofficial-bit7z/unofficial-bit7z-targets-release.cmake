#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "unofficial::bit7z::bit7z64" for configuration "Release"
set_property(TARGET unofficial::bit7z::bit7z64 APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(unofficial::bit7z::bit7z64 PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/bit7z64.lib"
  )

list(APPEND _cmake_import_check_targets unofficial::bit7z::bit7z64 )
list(APPEND _cmake_import_check_files_for_unofficial::bit7z::bit7z64 "${_IMPORT_PREFIX}/lib/bit7z64.lib" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
