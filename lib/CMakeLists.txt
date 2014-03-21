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
    aes_armor.cpp
    dh.cpp
    host.cpp
    identity.cpp
    key_responder.cpp
    comm/socket.cpp
    comm/udp_socket.cpp
    comm/socket_endpoint.cpp
    socket_channel.cpp
    socket_receiver.cpp
    socket_host_state.cpp
    protocol.cpp
    server.cpp
    stream.cpp
    stream_channel.cpp
    stream_peer.cpp
    timer.cpp
    ${simulator_SOURCES}
    ${platform_SOURCES})
