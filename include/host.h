//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include "dh.h"
#include "link.h"
#include "timer.h"
#include "identity.h"
#include "negotiation/key_responder.h"
#include "private/stream_host_state.h"
#include "logging.h"

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
    , public link_host_state
    , public dh_host_state
    , public key_host_state
    , public identity_host_state
    , public stream_host_state
    , public virtual asio_host_state
    , public timer_host_state
{
public:
    /**
     * Create a "bare-bones" host state object with no links or identity.
     * Client must establish a host identity via set_host_identity()
     * and activate one or more network links before using ssu.
     */
    explicit host() {}

    /**
     * Factory functions. Use those to create host instance.
     */
    static std::shared_ptr<host> create();
    static std::shared_ptr<host> create(settings_provider* settings, uint16_t default_port);

    ~host() { logger::debug() << this << " ~host"; }

    inline std::shared_ptr<host> get_host() override { return shared_from_this(); }
};

}
