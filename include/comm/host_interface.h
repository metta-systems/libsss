#pragma once

namespace uia {
namespace comm {

typedef uint32_t magic_t;
typedef uint8_t channel_number;

class socket;
class socket_receiver;
class socket_channel;

} // comm namespace
} // uia namespace

// Interface used by socket layer to work with the host state.
// Must be implemented by real host implementation, for example the one in ssu.
class comm_host_interface
{
public:
    // Interface used by socket to register itself on the host.
    virtual void activate_socket(uia::comm::socket*) = 0;
    virtual void deactivate_socket(uia::comm::socket*) = 0;

    // Interface to bind and lookup receivers based on channel magic value.
    virtual void bind_receiver(magic_t, uia::comm::socket_receiver*) = 0;
    virtual void unbind_receiver(magic_t) = 0;
    virtual bool has_receiver_for(magic_t) = 0;
    virtual uia::comm::socket_receiver* receiver(magic_t) = 0;
};

