cmake_minimum_required(VERSION 3.10)
project(xserializer_unit_test LANGUAGES CXX)
set(TARGET_PROJECT "xserializer_unit_test")
set(COMPONENT_REGISTRY "")

# Fetch Common.cmake from xcmake_tools
include(FetchContent)
FetchContent_Declare(
  xcmake_tools
  GIT_REPOSITORY https://github.com/LIONant-depot/xcmake_tools.git
  GIT_TAG main
  GIT_SHALLOW TRUE
  SOURCE_DIR "${CMAKE_BINARY_DIR}/_deps/xcmake_tools"
)
FetchContent_MakeAvailable(xcmake_tools)
include(${CMAKE_BINARY_DIR}/_deps/xcmake_tools/Common.cmake)

# Define the main executable
add_executable(${TARGET_PROJECT}
  "source/unittest/main.cpp"
  "source/unittest/xserializer_unittest.h"
)

# Organize source files in IDE
source_group("unit_test" FILES
  "source/unittest/xserializer_unittest.h"
)
source_group("" FILES
  "source/unittest/main.cpp"
)

# Add dependency subdirectory
add_subdirectory("build/dependency" "${CMAKE_CURRENT_BINARY_DIR}/xserializer")

# Process components
ProcessComponents()