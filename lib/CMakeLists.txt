set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations")

set(simulator_SOURCES
    simulation/sim_connection.cpp
    simulation/sim_host.cpp
    simulation/sim_link.cpp
    simulation/sim_packet.cpp
    simulation/sim_timer_engine.cpp
    simulation/simulator.cpp)

add_library(ssu STATIC 
    abstract_stream.cpp
    base_stream.cpp
    crypto/sign_key.cpp
    crypto/utils.cpp
    dh.cpp
    identity.cpp
    key_responder.cpp
    link.cpp
    protocol.cpp
    server.cpp
    stream.cpp
    stream_peer.cpp
    timer.cpp
    ${simulator_SOURCES})
