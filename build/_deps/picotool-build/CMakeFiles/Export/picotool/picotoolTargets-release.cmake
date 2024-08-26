#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "picotool" for configuration "Release"
set_property(TARGET picotool APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(picotool PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/picotool/picotool"
  )

list(APPEND _IMPORT_CHECK_TARGETS picotool )
list(APPEND _IMPORT_CHECK_FILES_FOR_picotool "${_IMPORT_PREFIX}/picotool/picotool" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
