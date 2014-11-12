//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include "sodiumpp/sodiumpp.h"
#include "sss/channel_armor.h"

namespace sss {

class cryptobox_armor : public sss::channel_armor
{
    using nonce64 = sodiumpp::nonce<crypto_box_NONCEBYTES-8, 8>;
    sodiumpp::boxer<nonce64> boxer_;
    sodiumpp::unboxer<nonce64> unboxer_;

public:
    cryptobox_armor(secret_key local_key, public_key remote_key);

    /**
     * Encode and authenticate data packet.
     * @param  pktseq Packet sequence number.
     * @param  pkt    Packet to send.
     * @return        Encoded and authenticated packet.
     */
    byte_array transmit_encode(uint64_t pktseq, byte_array const& pkt) override;

    /**
     * Decode packet in place.
     * @param  pktseq Packet sequence number.
     * @param  pkt    Packet to decrypt, decrypted packet returned in the same argument.
     * @return        true if packet is verified to be authentic.
     */
    bool receive_decode(uint64_t pktseq, byte_array& pkt) override;
};

} // sss namespace
