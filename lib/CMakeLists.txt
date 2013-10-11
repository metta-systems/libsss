set(simulator_SOURCES
    simulation/sim_connection.cpp
    simulation/sim_host.cpp
    simulation/sim_link.cpp
    simulation/sim_packet.cpp
    simulation/sim_timer_engine.cpp
    simulation/simulator.cpp)

set(platform_SOURCES)
if (UNIX)
    list(APPEND platform_SOURCES
        platform-unix.cpp)
endif()
if (WIN32)
    list(APPEND platform_SOURCES
        platform-win.cpp)
endif()

add_library(ssu STATIC 
    abstract_stream.cpp
    base_stream.cpp
    datagram_stream.cpp
    channel.cpp
    crypto/aes_armor.cpp
    crypto/aes_128_ctr.cpp
    crypto/aes_256_cbc.cpp
    crypto/sign_key.cpp
    crypto/rsa160_key.cpp
    crypto/dsa160_key.cpp
    crypto/utils.cpp
    dh.cpp
    host.cpp
    identity.cpp
    key_responder.cpp
    link.cpp
    link_channel.cpp
    protocol.cpp
    server.cpp
    stream.cpp
    stream_channel.cpp
    stream_peer.cpp
    timer.cpp
    ${simulator_SOURCES}
    ${platform_SOURCES})
