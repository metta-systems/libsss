//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include "arsenal/byte_array.h"

namespace sss {

/**
 * Abstract base class for channel encryption and authentication schemes.
 */
class channel_armor
{
    friend class channel;

protected:
    virtual byte_array transmit_encode(uint64_t pktseq, byte_array const& pkt) = 0;
    virtual bool receive_decode(uint64_t pktseq, byte_array& pkt) = 0;

public: // for unique_ptr
    virtual ~channel_armor();
};

} // sss namespace
