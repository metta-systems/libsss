//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "ssu/cryptobox_armor.h"
#include "sodiumpp/sodiumpp.h"
#include "arsenal/opaque_endian.h"
#include "arsenal/logging.h"
#include "ssu/channel.h"

using namespace sodiumpp;

namespace ssu {

cryptobox_armor::cryptobox_armor(secret_key local_key, public_key remote_key)
    : boxer_(remote_key, local_key)
    , unboxer_(remote_key, local_key)
{}

byte_array cryptobox_armor::transmit_encode(uint64_t pktseq, byte_array const& pkt)
{
    return boxer_.box(pkt);
}

bool cryptobox_armor::receive_decode(uint64_t pktseq, byte_array& pkt)
{
    try {
        pkt = unboxer_.unbox(pkt);
    }
    catch (char const* err) {
        logger::error() << err;
        return false;
    }
    return true;
}

} // ssu namespace
