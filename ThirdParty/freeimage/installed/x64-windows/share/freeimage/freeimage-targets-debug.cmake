#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "freeimage::FreeImage" for configuration "Debug"
set_property(TARGET freeimage::FreeImage APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(freeimage::FreeImage PROPERTIES
  IMPORTED_IMPLIB_DEBUG "${_IMPORT_PREFIX}/debug/lib/FreeImaged.lib"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/debug/bin/FreeImaged.dll"
  )

list(APPEND _cmake_import_check_targets freeimage::FreeImage )
list(APPEND _cmake_import_check_files_for_freeimage::FreeImage "${_IMPORT_PREFIX}/debug/lib/FreeImaged.lib" "${_IMPORT_PREFIX}/debug/bin/FreeImaged.dll" )

# Import target "freeimage::FreeImagePlus" for configuration "Debug"
set_property(TARGET freeimage::FreeImagePlus APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(freeimage::FreeImagePlus PROPERTIES
  IMPORTED_IMPLIB_DEBUG "${_IMPORT_PREFIX}/debug/lib/FreeImagePlusd.lib"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/debug/bin/FreeImagePlusd.dll"
  )

list(APPEND _cmake_import_check_targets freeimage::FreeImagePlus )
list(APPEND _cmake_import_check_files_for_freeimage::FreeImagePlus "${_IMPORT_PREFIX}/debug/lib/FreeImagePlusd.lib" "${_IMPORT_PREFIX}/debug/bin/FreeImagePlusd.dll" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
