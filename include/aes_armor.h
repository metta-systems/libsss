//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include "channel_armor.h"
#include "crypto/aes_128_ctr.h"

namespace ssu {

/**
 * AES protection with encryption and authentication.
 */
class aes_armor : public channel_armor
{
    aes_128_ctr tx_aes_;
    aes_128_ctr rx_aes_;
    byte_array  tx_mac_key_;
    byte_array  rx_mac_key_;

public:
    aes_armor(byte_array const& tx_enc_key, byte_array const& tx_mac_key,
              byte_array const& rx_enc_key, byte_array const& rx_mac_key);

    /**
     * Encode and authenticate data packet.
     * @param  pktseq Packet sequence number.
     * @param  pkt    Packet to send.
     * @return        Encoded and authenticated packet.
     */
    byte_array transmit_encode(uint64_t pktseq, const byte_array& pkt) override;

    /**
     * Decode packet in place.
     * @param  pktseq Packet sequence number.
     * @param  pkt    Packet to decrypt, decrypted packet returned in the same argument.
     * @return        true if packet is verified to be authentic.
     */
    bool receive_decode(uint64_t pktseq, byte_array& pkt) override;
};

} // ssu namespace
