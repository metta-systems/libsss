//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include "ssu/dh.h"
#include "ssu/timer.h"
#include "ssu/identity.h"
#include "ssu/negotiation/key_host_state.h"
#include "ssu/internal/stream_host_state.h"
#include "ssu/routing_host_state.h"
#include "ssu/socket_host_state.h"
#include "arsenal/logging.h"

class settings_provider;

namespace ssu {

/**
 * This class encapsulates all per-host state used by the ssu protocol.
 * By centralizing this state here instead of using global/static variables,
 * the host environment can be virtualized for simulation purposes
 * and multiple ssu instances can be run in one process.
 *
 * It is the client's responsibility to ensure that a host object
 * is not destroyed while any ssu objects still refer to it.
 *
 * Example: it is customary to create a shared_ptr to host.
 * @snippet doc/snippets.cpp Creating a host
 */
class host
    : public std::enable_shared_from_this<host>
    , public socket_host_state
    , public dh_host_state
    , public key_host_state
    , public identity_host_state
    , public stream_host_state
    , public virtual asio_host_state
    , public timer_host_state
    , public routing_host_state
{
public:
    // @todo Hide the constructor.
    explicit host() {}
    ~host() { logger::debug() << this << " ~host"; }
    inline std::shared_ptr<host> get_host() override { return shared_from_this(); }

    /**
     * @name Factory functions.
     * Use those to create host instance.
     */
    /**@{*/
    /**
     * Create a "bare-bones" host state object with no links or identity.
     * Client must establish a host identity via set_host_identity()
     * and activate one or more network links before using ssu.
     */
    static std::shared_ptr<host> create();
    /**
     * Create an easy-to-use default Host object. Uses the provided setting_provider
     * registry to locate, or create if necessary, a persistent host identity,
     * as described for identity_host_state::init_identity().
     * Also creates and binds to at least one UDP link, using a UDP port number specified
     * in the settings_provider, or defaulting to @a default_port if none.
     * If the desired UDP port cannot be bound, just picks an arbitrary UDP port instead.
     */
    static std::shared_ptr<host> create(settings_provider* settings,
        uint16_t default_port = stream_protocol::default_port);
    // Overload to make calls simpler.
    static inline std::shared_ptr<host> create(std::shared_ptr<settings_provider> settings,
        uint16_t default_port = stream_protocol::default_port)
    {
        return create(settings.get(), default_port);
    }
    /**@{*/
};

}
