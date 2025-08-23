include(${CMAKE_BINARY_DIR}/_deps/xcmake_tools/Common.cmake)

# Fetch dependencies
FetchAndPopulate("https://github.com/LIONant-depot/xerr.git" "main")
FetchAndPopulate("https://github.com/LIONant-depot/xcompression.git" "main")
FetchAndPopulate("https://github.com/LIONant-depot/xfile.git" "main")

DefineInterfaceComponent(xserializer "dependencies/xcore"
  "source/xserializer.h"
  "source/xserializer.cpp"
  "README.md"
)