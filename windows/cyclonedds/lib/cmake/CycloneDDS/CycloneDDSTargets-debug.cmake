#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "CycloneDDS::idlc" for configuration "Debug"
set_property(TARGET CycloneDDS::idlc APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(CycloneDDS::idlc PROPERTIES
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/bin/idlc.exe"
  )

list(APPEND _IMPORT_CHECK_TARGETS CycloneDDS::idlc )
list(APPEND _IMPORT_CHECK_FILES_FOR_CycloneDDS::idlc "${_IMPORT_PREFIX}/bin/idlc.exe" )

# Import target "CycloneDDS::idl" for configuration "Debug"
set_property(TARGET CycloneDDS::idl APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(CycloneDDS::idl PROPERTIES
  IMPORTED_IMPLIB_DEBUG "${_IMPORT_PREFIX}/lib/cycloneddsidl.lib"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/bin/cycloneddsidl.dll"
  )

list(APPEND _IMPORT_CHECK_TARGETS CycloneDDS::idl )
list(APPEND _IMPORT_CHECK_FILES_FOR_CycloneDDS::idl "${_IMPORT_PREFIX}/lib/cycloneddsidl.lib" "${_IMPORT_PREFIX}/bin/cycloneddsidl.dll" )

# Import target "CycloneDDS::ddsc" for configuration "Debug"
set_property(TARGET CycloneDDS::ddsc APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(CycloneDDS::ddsc PROPERTIES
  IMPORTED_IMPLIB_DEBUG "${_IMPORT_PREFIX}/lib/ddsc.lib"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/bin/ddsc.dll"
  )

list(APPEND _IMPORT_CHECK_TARGETS CycloneDDS::ddsc )
list(APPEND _IMPORT_CHECK_FILES_FOR_CycloneDDS::ddsc "${_IMPORT_PREFIX}/lib/ddsc.lib" "${_IMPORT_PREFIX}/bin/ddsc.dll" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
