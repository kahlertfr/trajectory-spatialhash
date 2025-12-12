#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "trajectory_spatialhash::trajectory_spatialhash" for configuration "Release"
set_property(TARGET trajectory_spatialhash::trajectory_spatialhash APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(trajectory_spatialhash::trajectory_spatialhash PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libtrajectory_spatialhash.a"
  )

list(APPEND _cmake_import_check_targets trajectory_spatialhash::trajectory_spatialhash )
list(APPEND _cmake_import_check_files_for_trajectory_spatialhash::trajectory_spatialhash "${_IMPORT_PREFIX}/lib/libtrajectory_spatialhash.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
