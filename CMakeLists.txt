cmake_minimum_required(VERSION 2.8)
project(libssu)

if (NOT BUILD_IN_METTANODE)
    include(cmake/setup.cmake)
    include_directories(3rdparty/endian 3rdparty/support/include)
    include_directories(3rdparty/krypto/include 3rdparty/plist/include)
    include_directories(3rdparty/routing/include)
endif()

include_directories(include)
add_subdirectory(doc)
add_subdirectory(lib)

# Build libsupport in standalone version.
if (NOT BUILD_IN_METTANODE)
    add_subdirectory(3rdparty/plist)
    add_subdirectory(3rdparty/support/lib)
    add_subdirectory(3rdparty/krypto/lib)
    add_subdirectory(3rdparty/routing/lib)
endif()

if (BUILD_TESTING)
    add_subdirectory(tests)
endif (BUILD_TESTING)

# Very simple secure shell terminal
if (UNIX AND BUILD_TESTING)
    #add_subdirectory(shell)
endif (UNIX AND BUILD_TESTING)
