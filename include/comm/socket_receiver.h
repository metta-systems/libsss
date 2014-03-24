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
#include "ssu/protocol.h"

class byte_array;

namespace uia {
namespace comm {

class socket_endpoint;

class receiver_host_interface
{
public:
    virtual void bind_receiver(magic_t, socket_receiver*) = 0;
    virtual void unbind_receiver(magic_t) = 0;
    virtual bool has_receiver_for(magic_t) = 0;
};

/**
 * Abstract base class for control protocol receivers.
 * Provides support for receiving control messages for registered protocol types.
 *
 * A control protocol is identified by a 32-bit magic value,
 * whose topmost byte must be zero to distinguish it from channels.
 */
class socket_receiver
{
    receiver_host_interface* host_interface_{nullptr};
    magic_t magic_{0};

protected:
    inline socket_receiver(receiver_host_interface* hi) : host_interface_(hi)
    {}

    inline socket_receiver(receiver_host_interface* host, magic_t magic) : host_interface_(hi) {
        bind(magic);
    }

    inline ~socket_receiver() {
        unbind();
    }

    void bind(magic_t magic);
    void unbind();

    inline magic_t magic() const {
        return magic_;
    }

    inline bool is_bound() const {
        return magic_ != 0;
    }

    virtual std::shared_ptr<host> get_host() {
        return host_;
    }

    // @fixme Possibly set_magic() might set a magic on default socket_receiver and bind() it.
    // just use bind(magic);

public:
    /**
     * Link calls this method to dispatch control messages.
     * @param msg Data packet.
     * @param src Origin endpoint.
     */
    virtual void receive(byte_array const& msg, uia::comm::socket_endpoint const& src) = 0;
};

} // comm namespace
} // uia namespace
