//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include "ssu/channel_armor.h"
#include "sodiumpp/sodiumpp.h"

namespace ssu {

class secretbox_armor : public ssu::channel_armor
{
    typedef sodiumpp::nonce<crypto_box_NONCEBYTES-8, 8> nonce64;
    std::string tx_key_;
    std::string rx_key_;

public:
    secretbox_armor(std::string tx_key, std::string rx_key);

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

} // ssu namespace
