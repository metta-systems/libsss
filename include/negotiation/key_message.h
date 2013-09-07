//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <boost/optional/optional.hpp>
#include "logging.h"
#include "flurry.h"
#include "underlying.h"
#include "opaque_endian.h"

namespace ssu {
namespace negotiation {

/**
All the XDR crap for key negotiation:
////////// Lightweight Checksum Negotiation //////////

// Checksum negotiation chunks
struct KeyChunkChkI1Data {
    // XXX nonces should be 64-bit, to ensure USIDs unique over all time!
    unsigned int    cki;        // Initiator's checksum key
    unsigned char   chani;      // Initiator's channel number
    opaque      cookie<>;   // Responder's cookie, if any
    opaque      ulpi<>;     // Upper-level protocol data
    opaque      cpkt<>;     // Piggybacked channel packet
};
struct KeyChunkChkR1Data {
    unsigned int    cki;        // Initiator's checksum key, echoed
    unsigned int    ckr;        // Responder's checksum key,
                    // = 0 if cookie required
    unsigned char   chanr;      // Responder's channel number,
                    // 0 if cookie required
    opaque      cookie<>;   // Responder's cookie, if any
    opaque      ulpr<>;     // Upper-level protocol data
    opaque      cpkt<>;     // Piggybacked channel packet
};

////////// Diffie-Helman Key Negotiation //////////

// Encrypted and authenticated identity blocks for I2 and R2 messages
struct KeyIdentI {
    unsigned char   chani;      // Initiator's channel number
    opaque      eidi<256>;  // Initiator's endpoint identifier
    opaque      eidr<256>;  // Desired EID of responder
    opaque      idpki<>;    // Initiator's identity public key
    opaque      sigi<>;     // Initiator's parameter signature
    opaque      ulpi<>;     // Upper-level protocol data
};
struct KeyIdentR {
    unsigned char   chanr;      // Responder's channel number
    opaque      eidr<256>;  // Responder's endpoint identifier
    opaque      idpkr<>;    // Responder's identity public key
    opaque      sigr<>;     // Responder's parameter signature
    opaque      ulpr<>;     // Upper-level protocol data
};

// Responder DH key signing block for R2 messages (JFKi only)
struct KeyParamR {
    DhGroup     group;      // DH group for public keys
    opaque      dhr<384>;   // Responder's DH public key
};

union KeyChunkUnion switch (KeyChunkType type) {
    case KeyChunkPacket:    opaque packet<>;

    case KeyChunkChkI1: KeyChunkChkI1Data chki1;
    case KeyChunkChkR1: KeyChunkChkR1Data chkr1;

    case KeyChunkDhI1:  KeyChunkDhI1Data dhi1;
    case KeyChunkDhR1:  KeyChunkDhR1Data dhr1;
    case KeyChunkDhI2:  KeyChunkDhI2Data dhi2;
    case KeyChunkDhR2:  KeyChunkDhR2Data dhr2;
};
typedef KeyChunkUnion ?KeyChunk;


// Top-level format of all negotiation protocol messages
struct KeyMessage {
    int     magic;      // 24-bit magic value
    KeyChunk    chunks<>;   // Message chunks
};

*/

class packet_chunk
{
    public://temp
};

inline flurry::oarchive& operator << (flurry::oarchive& oa, packet_chunk& pc)
{
    return oa;
}

inline flurry::iarchive& operator >> (flurry::iarchive& ia, packet_chunk& pc)
{
    return ia;
}

class checksum_init_chunk
{
    public://temp
};

inline flurry::oarchive& operator << (flurry::oarchive& oa, checksum_init_chunk& cic)
{
    return oa;
}

inline flurry::iarchive& operator >> (flurry::iarchive& ia, checksum_init_chunk& cic)
{
    return ia;
}

class checksum_response_chunk
{
    public://temp
};

inline flurry::oarchive& operator << (flurry::oarchive& oa, checksum_response_chunk& crc)
{
    return oa;
}

inline flurry::iarchive& operator >> (flurry::iarchive& ia, checksum_response_chunk& crc)
{
    return ia;
}

enum class dh_group_type : uint32_t {
    dh_group_1024 = 0, // 1024-bit DH group
    dh_group_2048 = 1, // 2048-bit DH group
    dh_group_3072 = 2, // 3072-bit DH group
    dh_group_max  = 3
};

// DH/JFK negotiation chunks

class dh_init1_chunk
{
    public://temp
    dh_group_type  group{dh_group_type::dh_group_1024};  // DH group of initiator's public key
    uint32_t       key_min_length{0};                    // Minimum AES key length: 16, 24, 32
    // fixme: replace these with fixed-size boost::array<>s?
    byte_array     initiator_hashed_nonce;               // Initiator's SHA256-hashed nonce
    byte_array     initiator_dh_public_key;              // Initiator's DH public key
    byte_array     responder_eid;                        // Optional: desired EID of responder
};

inline flurry::oarchive& operator << (flurry::oarchive& oa, dh_init1_chunk& dc1)
{
    oa << dc1.group
       << dc1.key_min_length
       << dc1.initiator_hashed_nonce
       << dc1.initiator_dh_public_key
       << dc1.responder_eid;
    return oa;
}

inline flurry::iarchive& operator >> (flurry::iarchive& ia, dh_init1_chunk& dc1)
{
    ia >> dc1.group
       >> dc1.key_min_length
       >> dc1.initiator_hashed_nonce
       >> dc1.initiator_dh_public_key
       >> dc1.responder_eid;
    return ia;
}

class dh_response1_chunk
{
    public://temp
    dh_group_type  group;                       // DH group for public keys
    int            key_min_length;              // Chosen AES key length: 16, 24, 32
    byte_array     initiator_hashed_nonce;      // Initiator's hashed nonce
    byte_array     responder_nonce;             // Responder's nonce
    byte_array     responder_dh_public_key;     // Responder's DH public key
    byte_array     responder_challenge_cookie;  // Responder's challenge cookie
    byte_array     responder_eid;               // Optional: responder's EID
    byte_array     responder_public_key;        // Optional: responder's public key
    byte_array     responder_signature;         // Optional: responder's signature
};

inline flurry::oarchive& operator << (flurry::oarchive& oa, dh_response1_chunk& dc1)
{
    oa << dc1.group
       << dc1.key_min_length
       << dc1.initiator_hashed_nonce
       << dc1.responder_nonce
       << dc1.responder_dh_public_key
       << dc1.responder_challenge_cookie
       << dc1.responder_eid
       << dc1.responder_public_key
       << dc1.responder_signature;
    return oa;
}

inline flurry::iarchive& operator >> (flurry::iarchive& ia, dh_response1_chunk& dc1)
{
    ia >> dc1.group
       >> dc1.key_min_length
       >> dc1.initiator_hashed_nonce
       >> dc1.responder_nonce
       >> dc1.responder_dh_public_key
       >> dc1.responder_challenge_cookie
       >> dc1.responder_eid
       >> dc1.responder_public_key
       >> dc1.responder_signature;
    return ia;
}


class dh_init2_chunk
{
    public://temp
    dh_group_type  group;                       // DH group for public keys
    int            key_min_length;              // AES key length: 16, 24, or 32
    byte_array     initiator_nonce;             // Initiator's original nonce
    byte_array     responder_nonce;             // Responder's nonce
    byte_array     initiator_dh_public_key;     // Initiator's DH public key
    byte_array     responder_dh_public_key;     // Responder's DH public key
    byte_array     responder_challenge_cookie;  // Responder's challenge cookie
    byte_array     initiator_id;                // Initiator's encrypted identity
};

inline flurry::oarchive& operator << (flurry::oarchive& oa, dh_init2_chunk& dc2)
{
    oa << dc2.group
       << dc2.key_min_length
       << dc2.initiator_nonce
       << dc2.responder_nonce
       << dc2.initiator_dh_public_key
       << dc2.responder_dh_public_key
       << dc2.responder_challenge_cookie
       << dc2.initiator_id;
    return oa;
}

inline flurry::iarchive& operator >> (flurry::iarchive& ia, dh_init2_chunk& dc2)
{
    ia >> dc2.group
       >> dc2.key_min_length
       >> dc2.initiator_nonce
       >> dc2.responder_nonce
       >> dc2.initiator_dh_public_key
       >> dc2.responder_dh_public_key
       >> dc2.responder_challenge_cookie
       >> dc2.initiator_id;
    return ia;
}

class dh_response2_chunk
{
    public://temp
    byte_array  initiator_hashed_nonce;         // Initiator's original nonce
    byte_array  responder_id;                   // Responder's encrypted identity
};

inline flurry::oarchive& operator << (flurry::oarchive& oa, dh_response2_chunk& dc2)
{
    oa << dc2.initiator_hashed_nonce
       << dc2.responder_id;
    return oa;
}

inline flurry::iarchive& operator >> (flurry::iarchive& ia, dh_response2_chunk& dc2)
{
    ia >> dc2.initiator_hashed_nonce
       >> dc2.responder_id;
    return ia;
}


enum class key_chunk_type : uint32_t {
    packet             = 0x0001,
    checksum_init      = 0x0011,
    checksum_response  = 0x0012,
    dh_init1           = 0x0021,
    dh_response1       = 0x0022,
    dh_init2           = 0x0023,
    dh_response2       = 0x0024,
};

class key_chunk
{
    public://temp
    key_chunk_type                             type;
    boost::optional<packet_chunk>              packet;
    boost::optional<checksum_init_chunk>       checksum_init;
    boost::optional<checksum_response_chunk>   checksum_response;
    boost::optional<dh_init1_chunk>            dh_init1;
    boost::optional<dh_response1_chunk>        dh_response1;
    boost::optional<dh_init2_chunk>            dh_init2;
    boost::optional<dh_response2_chunk>        dh_response2;
};

inline flurry::oarchive& operator << (flurry::oarchive& oa, key_chunk& kc)
{
    oa << kc.type;
    switch (kc.type) {
        case key_chunk_type::packet:
            oa << *kc.packet;
            break;
        case key_chunk_type::checksum_init:
            oa << *kc.checksum_init;
            break;
        case key_chunk_type::checksum_response:
            oa << *kc.checksum_response;
            break;
        case key_chunk_type::dh_init1:
            oa << *kc.dh_init1;
            break;
        case key_chunk_type::dh_response1:
            oa << *kc.dh_response1;
            break;
        case key_chunk_type::dh_init2:
            oa << *kc.dh_init2;
            break;
        case key_chunk_type::dh_response2:
            oa << *kc.dh_response2;
            break;
    }
    return oa;
}

inline flurry::iarchive& operator >> (flurry::iarchive& ia, key_chunk& kc)
{
    uint32_t type;
    ia >> type;
    kc.type = key_chunk_type(type);
    switch (kc.type) {
        case key_chunk_type::packet:
        {
            kc.packet = packet_chunk();
            ia >> *kc.packet;
            break;
        }
        case key_chunk_type::checksum_init:
        {
            kc.checksum_init = checksum_init_chunk();
            ia >> *kc.checksum_init;
            break;
        }
        case key_chunk_type::checksum_response:
        {
            kc.checksum_response = checksum_response_chunk();
            ia >> *kc.checksum_response;
            break;
        }
        case key_chunk_type::dh_init1:
        {
            kc.dh_init1 = dh_init1_chunk();
            ia >> *kc.dh_init1;
            break;
        }
        case key_chunk_type::dh_response1:
        {
            kc.dh_response1 = dh_response1_chunk();
            ia >> *kc.dh_response1;
            break;
        }
        case key_chunk_type::dh_init2:
        {
            kc.dh_init2 = dh_init2_chunk();
            ia >> *kc.dh_init2;
            break;
        }
        case key_chunk_type::dh_response2:
        {
            kc.dh_response2 = dh_response2_chunk();
            ia >> *kc.dh_response2;
            break;
        }
    }
    return ia;
}

class key_message
{
    public://temp
    uint32_t magic;
    std::vector<key_chunk> chunks;

public:
};

inline flurry::oarchive& operator << (flurry::oarchive& oa, key_message& km)
{
    oa << km.magic << km.chunks;
    return oa;
}

inline flurry::iarchive& operator >> (flurry::iarchive& ia, key_message& km)
{
    ia >> km.magic >> km.chunks;
    return ia;
}

} // negotiation namespace
} // ssu namespace
