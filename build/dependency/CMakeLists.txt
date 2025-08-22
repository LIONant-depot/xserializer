# The library
cmake_minimum_required(VERSION 3.10)

# Set policy CMP0169 to OLD to allow FetchContent_Populate
if(POLICY CMP0169)
  cmake_policy(SET CMP0169 OLD)
endif()

#
# Populate xerr
#
FetchContent_Declare(
  xerr
  GIT_REPOSITORY https://github.com/LIONant-depot/xerr.git
  GIT_TAG        main
  SOURCE_DIR     "${CMAKE_SOURCE_DIR}/dependencies/xerr"  # Adjusted to go up one level
)

FetchContent_GetProperties(xerr)
if(NOT xerr_POPULATED)
  message(STATUS "Populating xerr...")
  FetchContent_Populate(xerr)
  
  # Include xerr's CMakeLists.txt
  add_subdirectory("${CMAKE_SOURCE_DIR}/dependencies/xerr/build/dependency" "${CMAKE_CURRENT_BINARY_DIR}/xerr")
endif()


#
# Populate xcompression
#
FetchContent_Declare(
  xcompression
  GIT_REPOSITORY https://github.com/LIONant-depot/xcompression.git
  GIT_TAG        main
  SOURCE_DIR     "${CMAKE_SOURCE_DIR}/dependencies/xcompression"  # Adjusted to go up one level
)

FetchContent_GetProperties(xcompression)
if(NOT xcompression_POPULATED)
  message(STATUS "Populating xcompression...")
  FetchContent_Populate(xcompression)
  
  # Include xcompression's CMakeLists.txt
  add_subdirectory("${CMAKE_SOURCE_DIR}/dependencies/xcompression/build/dependency" "${CMAKE_CURRENT_BINARY_DIR}/xcompression")
endif()



#
# Populate xfile
#
FetchContent_Declare(
  xfile
  GIT_REPOSITORY https://github.com/LIONant-depot/xfile.git
  GIT_TAG        main
  SOURCE_DIR     "${CMAKE_SOURCE_DIR}/dependencies/xfile"  # Adjusted to go up one level
)

FetchContent_GetProperties(xfile)
if(NOT xfile_POPULATED)
  message(STATUS "Populating xfile...")
  FetchContent_Populate(xfile)
  
  # Include xfile's CMakeLists.txt
  add_subdirectory("${CMAKE_SOURCE_DIR}/dependencies/xfile/build/dependency" "${CMAKE_CURRENT_BINARY_DIR}/xfile")
endif()



#
# Define as an interface library only if not already defined
#
get_property(is_defined GLOBAL PROPERTY XSERIALIZER_FILES DEFINED)
if(NOT is_defined)

  # determine the root of our library
  if(DEFINED TARGET_PROJECT AND "${TARGET_PROJECT}" STREQUAL "xserializer_unit_test")
    set(ROOT ".")
  else()
    set(ROOT "dependencies/xserializer")

    # set the include path
    set(XSERIALIZER_INCLUDE_LIST "${ROOT}")
  endif()

  # set the files required
  set(XSERIALIZER_FILES_LIST
    "${ROOT}/source/xserializer.h"
    "${ROOT}/source/xserializer.cpp"
    "${ROOT}/README.md"
  )

  # Set global properties
  set_property(GLOBAL PROPERTY XSERIALIZER_GROUP "dependencies/xcore/xserializer")
  set_property(GLOBAL PROPERTY XSERIALIZER_FILES "${XSERIALIZER_FILES_LIST}")
  set_property(GLOBAL PROPERTY XSERIALIZER_INCLUDES "${XSERIALIZER_INCLUDE_LIST}")
  set_property(GLOBAL PROPERTY XSERIALIZER_LINKER_PATHS "${XSERIALIZER_LINKER_PATHS}")
  set_property(GLOBAL APPEND PROPERTY COMPONENT_REGISTRY "XSERIALIZER")

endif()