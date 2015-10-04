//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <set>
#include <memory>
#include <unordered_map>
#include <boost/signals2/signal.hpp>
#include "arsenal/algorithm.h"
#include "uia/comm/socket.h"
#include "uia/comm/packet_receiver.h"
#include "sss/internal/asio_host_state.h"
#include "sss/framing/stream_protocol.h" // only for default port?
#include "sss/forward_ptrs.h"

class settings_provider;

namespace sss {

class host;

/**
 * This mixin class encapsulates socket-related part of host state.
 * @see host
 */
class socket_host_state : public virtual asio_host_state, public uia::comm::socket_host_interface
{
    using packet_receiver = uia::comm::packet_receiver;
    using socket_set      = std::set<uia::comm::socket_wptr, std::owner_less<uia::comm::socket_wptr>>;

    /**
     * Lookup table of all registered packet_receivers for this host, keyed on their magic.
     */
    std::unordered_map<uint64_t, uia::comm::packet_receiver_wptr> receivers_;
    /**
     * List of all currently-active sockets.
     */
    socket_set active_sockets_;
    /**
     * ipv4 socket created by init_socket(), if any.
     */
    uia::comm::socket_ptr primary_socket_;
    /**
     * ipv6 socket created by init_socket(), if any.
     */
    uia::comm::socket_ptr primary_socket6_;

protected:
    /**
     * Create a new network socket.
     * The default implementation creates a udp_socket,
     * but this may be overridden to virtualize the network.
     */
    virtual uia::comm::socket_ptr create_socket();

    /**
     * Initialize the socket this host instance uses to communicate.
     * It exits the application via abort() if socket creation fails.
     * @param settings     Settings provider for port number. If not null, init_socket() looks
     *                     for a 'port' key and uses it in place of the specified default
     *                     port if found. In any case, sets the 'port' key to the port
     *                     actually used.
     * @param default_port Default port number to bind to if 'port' key not found in @a settings.
     */
    void init_socket(settings_provider* settings,
                     uint16_t default_port = stream_protocol::default_port);

public:
    /*@{*/
    /*@name receiver_host_interface implementation */
    /**
     * Create a receiver and bind it to control channel magic.
     */
    void bind_receiver(uint64_t magic, uia::comm::packet_receiver_wptr receiver) override
    {
        // @todo: Will NOT replace existing element.
        receivers_.insert(std::make_pair(magic, receiver));
    }

    void unbind_receiver(uint64_t magic) override { receivers_.erase(magic); }

    bool has_receiver_for(uint64_t magic) override { return contains(receivers_, magic); }

    /**
     * Find and return a receiver for given control channel magic value.
     */
    uia::comm::packet_receiver_wptr receiver_for(uint64_t magic) override;
    /*@}*/

    /*@{*/
    /*@name comm_host_interface implementation */

    inline void activate_socket(uia::comm::socket_wptr swp) override
    {
        active_sockets_.insert(swp);
        on_active_sockets_changed();
    }

    inline void deactivate_socket(uia::comm::socket_wptr swp) override
    {
        active_sockets_.erase(swp);
        on_active_sockets_changed();
    }
    /*@}*/

    using asio_host_state::get_io_service;

    /**
     * Obtain a list of all currently active sockets.
     * Used by upper-level protocols (e.g., key exchange, registration)
     * to send out initial discovery messages on all available sockets.
     * Subsequent messages normally get sent only to
     * the specific socket a discovery response was seen on.
     * @return a set of pointers to each currently active socket.
     */
    inline socket_set active_sockets() const { return active_sockets_; }

    /**
     * Get a set of all known local endpoints for all active sockets.
     */
    std::unordered_set<uia::comm::endpoint> active_local_endpoints();

    using active_sockets_changed_signal = boost::signals2::signal<void(void)>;
    /**
     * This signal is sent whenever the host's set of active sockets changes.
     */
    active_sockets_changed_signal on_active_sockets_changed;
};

} // sss namespace
