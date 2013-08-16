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

/**
 * Abstract base class for control protocol receivers.
 * Provides support for receiving control messages for registered protocol types.
 *
 * A control protocol is identified by a 32-bit magic value,
 * whose topmost byte must be zero to distinguish it from channels.
 */
class link_receiver
{
    magic_t magic_{0};

protected:
    link_receiver() {}

    inline magic_t magic() const { return magic_; }

public:
    inline void magic(magic_t set_magic) { magic_ = set_magic; } //temp?
    virtual void receive(const byte_array& msg, const link_endpoint& src) = 0;
};

} // ssu namespace
