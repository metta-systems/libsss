//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

// #include <boost/endian/conversion.hpp>
#include <boost/optional/optional.hpp>
#include "logging.h"
#include "msgpack_ostream.h"
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

class checksum_init_chunk
{
    public://temp
};

class checksum_response_chunk
{
    public://temp
};

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

    MSGPACK_DEFINE(group, key_min_length, initiator_hashed_nonce, initiator_dh_public_key, responder_eid);
};

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

    MSGPACK_DEFINE(group, key_min_length, initiator_hashed_nonce, responder_nonce
          , responder_dh_public_key, responder_challenge_cookie, responder_eid
          , responder_public_key, responder_signature);
};

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

    MSGPACK_DEFINE(group, key_min_length, initiator_nonce, responder_nonce
          , initiator_dh_public_key, responder_dh_public_key, responder_challenge_cookie
          , initiator_id);
};

class dh_response2_chunk
{
    byte_array  initiator_hashed_nonce;         // Initiator's original nonce
    byte_array  responder_id;                   // Responder's encrypted identity

    MSGPACK_DEFINE(initiator_hashed_nonce, responder_id);
};

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

    MSGPACK_DEFINE(type);
    // friend class boost::serialization::access;
    // template<class Archive>
    // void load(Archive &ar, const unsigned int) {
    //     uint32_t t;
    //     ar >> t;
    //     type = key_chunk_type(boost::endian2::big(t));
    //     switch (type) {
    //         case key_chunk_type::packet:
    //         {
    //             packet = packet_chunk();
    //             ar >> *packet;
    //             break;
    //         }
    //         case key_chunk_type::checksum_init:
    //         {
    //             checksum_init = checksum_init_chunk();
    //             ar >> *checksum_init;
    //             break;
    //         }
    //         case key_chunk_type::checksum_response:
    //         {
    //             checksum_response = checksum_response_chunk();
    //             ar >> *checksum_response;
    //             break;
    //         }
    //         case key_chunk_type::dh_init1:
    //         {
    //             dh_init1 = dh_init1_chunk();
    //             ar >> *dh_init1;
    //             break;
    //         }
    //         case key_chunk_type::dh_response1:
    //         {
    //             dh_response1 = dh_response1_chunk();
    //             ar >> *dh_response1;
    //             break;
    //         }
    //         case key_chunk_type::dh_init2:
    //         {
    //             dh_init2 = dh_init2_chunk();
    //             ar >> *dh_init2;
    //             break;
    //         }
    //         case key_chunk_type::dh_response2:
    //         {
    //             dh_response2 = dh_response2_chunk();
    //             ar >> *dh_response2;
    //             break;
    //         }
    //     }
    // }

    // template <typename Packer>
    // inline void msgpack_pack(Packer& o) const
    // {
    //     o << type;
        // switch (type) {
        //     case key_chunk_type::packet:
        //         os << *packet;
        //         break;
        //     case key_chunk_type::checksum_init:
        //         os << *checksum_init;
        //         break;
        //     case key_chunk_type::checksum_response:
        //         os << *checksum_response;
        //         break;
        //     case key_chunk_type::dh_init1:
        //         os << *dh_init1;
        //         break;
        //     case key_chunk_type::dh_response1:
        //         os << *dh_response1;
        //         break;
        //     case key_chunk_type::dh_init2:
        //         os << *dh_init2;
        //         break;
        //     case key_chunk_type::dh_response2:
        //         os << *dh_response2;
        //         break;
        // }
    // }
};

class key_message
{
    public://temp
    uint32_t magic;
    std::vector<key_chunk> chunks;

    MSGPACK_DEFINE(magic, chunks);

public:
};

} // negotiation namespace
} // ssu namespace
