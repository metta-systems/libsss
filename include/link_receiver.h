//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include "protocol.h"

class byte_array;

namespace ssu {

class link_endpoint;
class link_host_state;

/**
 * Abstract base class for control protocol receivers.
 * Provides support for receiving control messages for registered protocol types.
 *
 * A control protocol is identified by a 32-bit magic value,
 * whose topmost byte must be zero to distinguish it from channels.
 */
class link_receiver
{
    link_host_state& host_;
    magic_t magic_{0};

protected:
    void bind();
    void unbind();

    inline link_receiver(link_host_state& host) : host_(host) {}
    inline link_receiver(link_host_state& host, magic_t magic) : host_(host), magic_(magic) { bind(); }
    inline ~link_receiver() { unbind(); }

    inline magic_t magic() const { return magic_; }

    // @fixme Possibly set_magic() might set a magic on default link_receiver and bind() it.

public:
    /**
     * Link calls this method to dispatch control messages.
     * @param msg Data packet.
     * @param src Origin endpoint.
     */
    virtual void receive(byte_array const& msg, link_endpoint const& src) = 0;
};

} // ssu namespace
