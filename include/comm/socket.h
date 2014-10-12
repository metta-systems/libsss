//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <map>
#include <string>
#include <vector>
#include "comm/socket_endpoint.h"
#include "comm/host_interface.h"

namespace uia {
namespace comm {

class socket_receiver;
class socket_channel;

/**
 * Abstract base class for entity connecting two endpoints using some network.
 * Socket manages connection lifetime and maintains the connection status info.
 * For connected sockets there may be a number of channels established.
 * Socket orchestrates initiation of key exchanges (@fixme should it?).
 */
class socket
{
    /**
     * Host state instance this socket is attached to.
     */
    comm_host_interface* host_interface_{nullptr};

    /**
     * Channels working through this socket at the moment.
     * Socket does NOT own the channels.
     * Channels are distinguished by sender's short-term public key.
     */
    std::map<std::string, std::weak_ptr<socket_channel>> channels_;

    /**
     * True if this socket is fair game for use by upper level protocols.
     */
    bool active_{false};

public:
    typedef std::weak_ptr<socket> weak_ptr;

    // sss expresses current socket status as one of three states:
    enum class status {
        down,    ///< definitely appears to be down.
        stalled, ///< briefly lost connectivity, but may be temporary.
        up       ///< apparently alive, all is well as far as we know.
    };

    static std::string status_string(status s);

    socket(comm_host_interface* hi) : host_interface_(hi) {}
    virtual ~socket();

    /**
     * Determine whether this socket is active.
     * Only active sockets are returned by socket_host_state::active_sockets().
     * @return true if socket is active.
     */
    inline bool is_active() const {
        return active_;
    }

    /**
     * Activate or deactivate this socket.
     * Only active socket are returned by socket_host_state::active_sockets().
     * @param active true if the socket should be marked active.
     */
    void set_active(bool active);

    /**
     * Open the underlying socket, bind it to given endpoint and activate it if successful.
     * @param  ep Endpoint on the local machine to bind the socket to.
     * @return    true if bind successfull, false otherwise.
     */
    virtual bool bind(endpoint const& ep) = 0;

    /**
     * Unbind and close the underlying socket.
     */
    virtual void unbind() = 0;

    /**
     * Send a packet on this socket.
     * @param ep the destination address to send the packet to.
     * @param data the packet data.
     * @param size the packet size.
     * @return true if send was successful.
     */
    virtual bool send(endpoint const& ep, char const* data, size_t size) = 0;

    /**
     * Send a packet on this socket.
     * This is an overridden function provided for convenience.
     * @param ep the destination address to send the packet to.
     * @param msg the packet data.
     * @return true if send was successful.
     */
    inline bool send(endpoint const& ep, byte_array const& msg) {
        return send(ep, msg.const_data(), msg.size());
    }

    /**
     * Find all known local endpoints referring to this socket.
     * @return a list of endpoint objects.
     */
    virtual std::vector<endpoint> local_endpoints() = 0;

    /**
     * Return local port number at which this socket is bound on the host.
     * @return local open port number.
     */
    virtual uint16_t local_port() = 0;

    /**
     * Return a description of any error detected on bind() or send().
     */
    virtual std::string error_string() = 0;

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

    /**
     * Returns true if this socket provides congestion control
     * when communicating with the specified remote endpoint.
     */
    virtual bool is_congestion_controlled(endpoint const& ep);

    /**
     * For congestion-controlled sockets, returns the number of bytes that may
     * be transmitted now to a particular target endpoint.
     */
    virtual size_t may_transmit(endpoint const& ep);

protected:
    /**
     * Implementation subclass calls this method with received packets.
     * @param msg the packet received.
     * @param src the source from which the packet arrived.
     */
    void receive(byte_array const& msg, socket_endpoint const& src);
};

} // comm namespace
} // uia namespace
