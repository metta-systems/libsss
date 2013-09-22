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

# Set some flags to keep code tidy
if (NOT BUILD_IN_METTANODE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-parameter -Wno-unused-private-field")
endif()

if (BUILD_TESTING)
    add_subdirectory(tests)
endif (BUILD_TESTING)
