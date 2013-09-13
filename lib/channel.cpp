//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "channel.h"

namespace ssu {

constexpr int channel::header_len;

channel::channel(std::shared_ptr<host> host)
    : link_channel()
    , host_(host)
{}

channel::~channel()
{}

void channel::start(bool initiate)
{}

void channel::stop()
{}

int channel::may_transmit()
{
    return 1;
}

bool channel::transmit_ack(byte_array &pkt, uint64_t ackseq, unsigned ackct)
{
    return false;
}

void channel::acknowledged(uint64_t txseq, int npackets, uint64_t rxackseq)
{}

void channel::missed(uint64_t txseq, int npackets)
{}

void channel::expire(uint64_t txseq, int npackets)
{}

void channel::receive(const byte_array& msg, const link_endpoint& src)
{}

} // ssu namespace
