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
        platform-unix.cpp
        comm/platform-unix.cpp)
endif()
if (WIN32)
    list(APPEND platform_SOURCES
        platform-win.cpp
        comm/platform-win.cpp)
endif()

set(comm_SOURCES
    comm/socket.cpp
    comm/udp_socket.cpp
    comm/socket_endpoint.cpp
    comm/socket_channel.cpp
    comm/socket_receiver.cpp)

set(stream_SOURCES
    abstract_stream.cpp
    base_stream.cpp
    datagram_stream.cpp
    stream.cpp
    stream_responder.cpp
    stream_channel.cpp
    stream_peer.cpp
    stream_protocol.cpp)

add_library(sss STATIC
    ${comm_SOURCES}
    ${stream_SOURCES}
    channel.cpp
    peer_identity.cpp
    kex_responder.cpp
    kex_initiator.cpp
    server.cpp
    timer.cpp
    host.cpp
    asio_host_state.cpp
    socket_host_state.cpp
    ${simulator_SOURCES}
    ${platform_SOURCES})
