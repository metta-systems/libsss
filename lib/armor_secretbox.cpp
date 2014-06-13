//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "ssu/armor_secretbox.h"
#include "krypto/krypto.h"
#include "arsenal/opaque_endian.h"
#include "arsenal/logging.h"
#include "ssu/channel.h"

namespace ssu {

secretbox_armor::secretbox_armor(std::string tx_key, std::string rx_key)
    : tx_key_(tx_key)
    , rx_key_(rx_key)
{}

byte_array aes_armor::transmit_encode(uint64_t pktseq, byte_array const& pkt)
{
    union {
        big_uint32_t words[6];
        boost::array<uint8_t, crypto_secretbox_NONCEBYTES> bytes;
    } ivec;

    // Build the initialization vector template for encryption.
    // We also use the first 8 bytes as a pseudo-header for the MAC.
    ivec.words[0] = pktseq >> 32;
    ivec.words[1] = pktseq;
    ivec.words[2] = 0x56584166;  // 'VXAf'
    ivec.words[3] = 0;  // per-packet block counter -- ??
    ivec.words[4] = 0;  // temp filler
    ivec.words[5] = 0;  // temp filler

    byte_array out = crypto_secretbox(pkt.as_string(), ivec.bytes, tx_key_);

    // Copy the unencrypted header (XX hack)
    // assert(encofs == 4);
    // *out.as<uint32_t>() = *pkt.as<uint32_t>();

    // Compute the MAC for the packet,
    // including the full 64-bit packet sequence number as a pseudo-header.
    // crypto::hash hmac(tx_mac_key_.as_vector());
    // hmac.update(byte_array::wrap((const char*)ivec.bytes.data(), 8).as_vector()); // XXX inefficient
    // hmac.update(out.as_vector());
    // crypto::hash::value result;
    // hmac.finalize(result);
    // out.append(result);
    // assert(out.size() == pkt.size() + crypto::HMACLEN);

    return out;
}

bool aes_armor::receive_decode(uint64_t pktseq, byte_array& pkt)
{
    union {
        big_uint32_t words[4];
        boost::array<uint8_t, AES_BLOCK_SIZE> bytes;
    } ivec;

    // if (pkt.size() - crypto::HMACLEN < ssu::channel::header_len) {
    //     logger::warning() << "Received packet too small.";
    //     return false;
    // }

    // Build the initialization vector template for decryption.
    // We also use the first 8 bytes as a pseudo-header for the MAC.
    ivec.words[0] = pktseq >> 32;
    ivec.words[1] = pktseq;
    ivec.words[2] = 0x56584166;  // 'VXAf'
    ivec.words[3] = 0;  // per-packet block counter -- ??
    ivec.words[4] = 0;  // temp filler
    ivec.words[5] = 0;  // temp filler

    byte_array out;
    try {
        out = crypto_secretbox_open(pkt.as_string(), ivec.bytes, rx_key_);
    }
    catch (...)
    {
        logger::warning() << "Received bad encrypted packet";
        return false;
    }

    // Copy the unencrypted header (XX hack)
    // assert(encofs == 4);
    // *out.as<uint32_t>() = *pkt.as<uint32_t>();

    pkt = out;

    return true;
}

} // ssu namespace
