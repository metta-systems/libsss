#pragma once

namespace uia {
namespace comm {

/**
 * Protocol magic marker, must have 0x00 as the highest byte (channel number).
 */
typedef uint32_t magic_t;

/**
 * An 8-bit channel number distinguishes different channels
 * between the same pair of link-layer endpoints. Channel number 0 is always invalid.
 * Up to 255 simultaneous channels possible.
 */
typedef uint8_t channel_number;

class socket;
class socket_receiver;
class socket_channel;

// Interface used by socket layer to work with the host state.
// Must be implemented by real host implementation, for example the one in sss.
class comm_host_interface
{
public:
    // Interface used by socket to register itself on the host.
    virtual void activate_socket(socket*) = 0;
    virtual void deactivate_socket(socket*) = 0;

    // Interface to bind and lookup receivers based on channel magic value.
    virtual void bind_receiver(magic_t, socket_receiver*) = 0;
    virtual void unbind_receiver(magic_t) = 0;
    virtual bool has_receiver_for(magic_t) = 0;
    virtual socket_receiver* receiver_for(magic_t) = 0;
};

} // comm namespace
} // uia namespace
