set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations")

add_library(ssu STATIC 
    abstract_stream.cpp
    base_stream.cpp
    identity.cpp
    key_responder.cpp
    link.cpp
    protocol.cpp
    server.cpp
    stream.cpp
    timer.cpp)
