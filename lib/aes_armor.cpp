//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "krypto/krypto.h"
#include "arsenal/opaque_endian.h"
#include "arsenal/logging.h"
#include "sss/channel.h"
#include "sss/channels/aes_armor.h"

// Info http://stackoverflow.com/questions/3141860/aes-ctr-256-encryption-mode-of-operation
// @todo use EVP_aes_128_ctr instead of manual

namespace sss {

channel_armor::~channel_armor()
{}

aes_armor::aes_armor(byte_array const& tx_enc_key, byte_array const& tx_mac_key,
                     byte_array const& rx_enc_key, byte_array const& rx_mac_key)
    : tx_aes_(tx_enc_key)
    , rx_aes_(rx_enc_key)
    , tx_mac_key_(tx_mac_key)
    , rx_mac_key_(rx_mac_key)
{}

byte_array aes_armor::transmit_encode(uint64_t pktseq, byte_array const& pkt)
{
    union {
        big_uint32_t words[4];
        boost::array<uint8_t, AES_BLOCK_SIZE> bytes;
    } ivec;

    // Build the initialization vector template for encryption.
    // We also use the first 8 bytes as a pseudo-header for the MAC.
    ivec.words[0] = pktseq >> 32;
    ivec.words[1] = pktseq;
    ivec.words[2] = 0x56584166;  // 'VXAf'
    ivec.words[3] = 0;  // per-packet block counter -- ??

    // Encrypt the block in CTR mode
    byte_array out = tx_aes_.encrypt(pkt, ivec.bytes);

    // Copy the unencrypted header (XX hack)
    // assert(encofs == 4);
    *out.as<uint32_t>() = *pkt.as<uint32_t>();

    // Compute the MAC for the packet,
    // including the full 64-bit packet sequence number as a pseudo-header.
    crypto::hash hmac(tx_mac_key_.as_vector());
    hmac.update(byte_array::wrap((const char*)ivec.bytes.data(), 8).as_vector()); // XXX inefficient
    hmac.update(out.as_vector());
    crypto::hash::value result;
    hmac.finalize(result);
    out.append(result);
    assert(out.size() == pkt.size() + crypto::HMACLEN);

    return out;
}

bool aes_armor::receive_decode(uint64_t pktseq, byte_array& pkt)
{
    union {
        big_uint32_t words[4];
        boost::array<uint8_t, AES_BLOCK_SIZE> bytes;
    } ivec;

    if (pkt.size() - crypto::HMACLEN < sss::channel::header_len) {
        logger::warning() << "Received packet too small.";
        return false;
    }

    // Build the initialization vector template for decryption.
    // We also use the first 8 bytes as a pseudo-header for the MAC.
    ivec.words[0] = pktseq >> 32;
    ivec.words[1] = pktseq;
    ivec.words[2] = 0x56584166;  // 'VXAf'
    ivec.words[3] = 0;  // per-packet block counter -- ??

    // Verify the packet's MAC.
    // @todo: wrap this into a hmac_verify() kind of helper
    crypto::hash hmac(rx_mac_key_.as_vector());
    hmac.update(byte_array::wrap((const char*)ivec.bytes.data(), 8).as_vector()); // XXX inefficient
    hmac.update(pkt.left(pkt.size() - crypto::HMACLEN).as_vector()); // XXX inefficient
    crypto::hash::value result;
    hmac.finalize(result);

    byte_array expected_hmac = pkt.right(crypto::HMACLEN);

    if (byte_array(result) != expected_hmac)
    {
        logger::warning() << "Received packet with bad MAC.";
        return false;
    }

    // Decrypt the block in CTR mode
    byte_array out = rx_aes_.decrypt(pkt, ivec.bytes);

    // Copy the unencrypted header (XX hack)
    // assert(encofs == 4);
    *out.as<uint32_t>() = *pkt.as<uint32_t>();

    out.resize(out.size() - crypto::HMACLEN);
    pkt = out;

    return true;
}

} // sss namespace
