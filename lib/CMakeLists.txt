set(simulator_SOURCES
    simulation/sim_connection.cpp
    simulation/sim_host.cpp
    simulation/sim_socket.cpp
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

set(stream_SOURCES
    abstract_stream.cpp
    base_stream.cpp
    datagram_stream.cpp
    stream.cpp
    stream_responder.cpp
    stream_channel.cpp
    stream_peer.cpp
    stream_protocol.cpp)

set(framing_SOURCES
    framing/framing.cpp
    framing/ack_frame.cpp
    framing/close_frame.cpp
    framing/decongestion_frame.cpp
    framing/priority_frame.cpp
    framing/reset_frame.cpp
    framing/settings_frame.cpp
    framing/stream_frame.cpp)

add_library(sss STATIC
    ${stream_SOURCES}
    channel.cpp
    server.cpp
    host.cpp
    ${simulator_SOURCES}
    ${platform_SOURCES}
    ${framing_SOURCES})
