set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations")

add_library(ssu STATIC 
    abstract_stream.cpp
    stream.cpp
    link.cpp
    protocol.cpp
    key_responder.cpp
    timer.cpp)
