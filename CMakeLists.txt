cmake_minimum_required(VERSION 2.8)
project(libsss)

if (NOT BUILD_IN_UVVY)
    include(cmake/setup.cmake)
    include_directories(3rdparty/endian 3rdparty/arsenal/include)
    include_directories(3rdparty/krypto/include)
    include_directories(3rdparty/routing/include)
    include_directories(3rdparty)
endif()

include_directories(include)
add_subdirectory(doc)
add_subdirectory(lib)

# Build libarsenal in standalone version.
if (NOT BUILD_IN_UVVY)
    add_subdirectory(3rdparty/arsenal/lib)
    add_subdirectory(3rdparty/krypto/lib)
    add_subdirectory(3rdparty/routing/lib)
endif()

if (BUILD_TESTING)
    add_subdirectory(tests)
endif (BUILD_TESTING)

# Very simple secure shell terminal
if (UNIX AND BUILD_TESTING)
#    add_subdirectory(tools/shell)
endif (UNIX AND BUILD_TESTING)
