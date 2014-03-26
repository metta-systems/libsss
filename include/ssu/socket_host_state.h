//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <boost/signals2/signal.hpp>
#include "ssu/asio_host_state.h"
#include "ssu/socket_receiver.h"
#include "arsenal/algorithm.h"
#include "comm/socket.h"

class settings_provider;

namespace ssu {

class host;

/**
 * This mixin class encapsulates socket-related part of host state.
 * @see host
 */
class socket_host_state : virtual public asio_host_state /* jeez, damn asio! */
    , public uia::comm::comm_host_interface
{
    /**
     * Lookup table of all registered socket_receiver for this host,
     * keyed on their 24-bit magic control packet type.
     */
    std::unordered_map<magic_t, socket_receiver*> receivers_;
    /**
     * List of all currently-active links.
     */
    std::unordered_set<uia::comm::socket*> active_sockets_;
    /**
     * ipv4 socket created by init_socket(), if any.
     */
    std::shared_ptr<uia::comm::socket> primary_socket_;
    /**
     * ipv6 socket created by init_socket(), if any.
     */
    std::shared_ptr<uia::comm::socket> primary_socket6_;

protected:
    /**
     * Create a new network socket.
     * The default implementation creates a udp_socket,
     * but this may be overridden to virtualize the network.
     */
    virtual std::shared_ptr<uia::comm::socket> create_socket();

    /**
     * Initialize the socket this host instance uses to communicate.
     * It exits the application via abort() if socket creation fails.
     * @param settings     Settings provider for port number. If not null, init_socket() looks
     *                     for a 'port' key and uses it in place of the specified default
     *                     port if found. In any case, sets the 'port' key to the port
     *                     actually used.
     * @param default_port Default port number to bind to if 'port' key not found in @a settings.
     * @return the created socket (during this or a previous call).
     */
    void init_socket(settings_provider* settings,
        uint16_t default_port = stream_protocol::default_port);

public:
    /*@{*/
    /**
     * Create a receiver and bind it to control channel magic.
     */
    void bind_receiver(magic_t magic, socket_receiver* receiver)
    {
        if (magic & 0xff000000) {
            throw "Invalid magic value for binding a receiver.";
        }
        receivers_.insert(std::make_pair(magic, receiver)); // @todo: Will NOT replace existing element.
    }

    void unbind_receiver(magic_t magic) {
        receivers_.erase(magic);
    }

    bool has_receiver_for(magic_t magic) {
        return contains(receivers_, magic);
    }
    /*@}*/

    /*@{*/
    /**
     * Find and return a receiver for given control channel magic value.
     */
    socket_receiver* receiver(magic_t magic) override;//@todo move to receiver_host_interface

    inline void activate_socket(uia::comm::socket* l) override
    {
        active_sockets_.insert(l);
        on_active_sockets_changed();
    }

    inline void deactivate_socket(uia::comm::socket* l) override
    {
        active_sockets_.erase(l);
        on_active_sockets_changed();
    }
    /*@}*/

    /**
     * Obtain a list of all currently active sockets.
     * Used by upper-level protocols (e.g., key exchange, registration)
     * to send out initial discovery messages on all available sockets.
     * Subsequent messages normally get sent only to
     * the specific socket a discovery response was seen on.
     * @return a set of pointers to each currently active socket.
     */
    inline std::unordered_set<uia::comm::socket*> active_sockets() const {
        return active_sockets_;
    }

    /**
     * Get a set of all known local endpoints for all active sockets.
     */
    std::unordered_set<uia::comm::endpoint> active_local_endpoints();

    typedef boost::signals2::signal<void(void)> active_sockets_changed_signal;
    /**
     * This signal is sent whenever the host's set of active sockets changes.
     */
    active_sockets_changed_signal on_active_sockets_changed;
};

} // ssu namespace

