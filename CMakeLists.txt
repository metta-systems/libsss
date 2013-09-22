cmake_minimum_required(VERSION 2.8)

include(cmake/setup.cmake)

# Additional unofficial boost libraries
include_directories(../../mettanode/3rdparty/endian)

include_directories(include ../support/include 3rdparty/msgpack/src)
add_subdirectory(doc)
add_subdirectory(lib)

# Build libsupport in standalone version.
if (NOT BUILD_IN_METTANODE)
    add_subdirectory(../support/lib ${CMAKE_CURRENT_BINARY_DIR}/support)
endif()

if (BUILD_TESTING)
    add_subdirectory(tests)
endif (BUILD_TESTING)
