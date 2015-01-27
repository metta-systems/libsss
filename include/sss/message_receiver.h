#pragma once

#include <boost/signals2/signal.hpp>
#include "comm/socket_endpoint.h"
#include "comm/packet_receiver.h"

// Get a message, dispatch to the right channel if exists.
class message_receiver : public public uia::comm::packet_receiver
{
    /**
     * Channels working through this socket at the moment.
     * Socket does NOT own the channels.
     * Channels are distinguished by sender's short-term public key.
     */
    std::map<std::string, std::weak_ptr<socket_channel>> channels_;

public:
    /**
     * Find channel attached to this socket.
     *
     * @todo channel_key should be enough without the src, since it's 32 bytes chances of collision
     * are negligible, and it might also keep working if other endpoint changes address.
     */
    std::weak_ptr<socket_channel> channel_for(std::string channel_key);

    /**
     * Bind a new socket_channel to this socket.
     * Called by socket_channel::bind() to register in the table of channels.
     */
    bool bind_channel(std::string channel_key, std::weak_ptr<socket_channel> lc);

    /**
     * Unbind a socket_channel associated with channel short-term key @a channel_key.
     * Called by socket_channel::unbind() to unregister from the table of channels.
     */
    void unbind_channel(std::string channel_key);

    void receive(byte_array const& msg, uia::comm::socket_endpoint const& src) override;
};
