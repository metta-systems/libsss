include_directories(.)
add_library(shell STATIC shell_protocol.cpp shell_stream.cpp)
add_subdirectory(client)
add_subdirectory(server)
