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
#include "protocol.h"

namespace ssu {
namespace negotiation {

//=================================================================================================
// packet_chunk
//=================================================================================================

struct packet_chunk
{
    byte_array data;
};

//-------------------------------------------------------------------------------------------------

inline flurry::oarchive& operator << (flurry::oarchive& oa, packet_chunk const& pc)
{
    oa << pc.data;
    return oa;
}

inline flurry::iarchive& operator >> (flurry::iarchive& ia, packet_chunk& pc)
{
    ia >> pc.data;
    return ia;
}

//=================================================================================================
// dh_group_type
//=================================================================================================

enum class dh_group_type : uint32_t {
    dh_group_1024 = 0, // 1024-bit DH group
    dh_group_2048 = 1, // 2048-bit DH group
    dh_group_3072 = 2, // 3072-bit DH group
    dh_group_max  = 3
};

//=================================================================================================
// Encrypted and authenticated identity blocks for init2 and response2 messages
//
// initiator_identity_chunk
//=================================================================================================

struct initiator_identity_chunk
{
    channel_number initiator_channel_number;
    byte_array     initiator_eid;
    byte_array     responder_eid; // Desired EID of responder
    byte_array     initiator_id_public_key;  // Initiator's identity public key
    byte_array     initiator_signature; // Initiator's parameter signature
    byte_array     user_data_in; // Upper-level protocol data
};

//-------------------------------------------------------------------------------------------------

inline flurry::oarchive& operator << (flurry::oarchive& oa, initiator_identity_chunk const& iic)
{
    oa << iic.initiator_channel_number
       << iic.initiator_eid
       << iic.responder_eid
       << iic.initiator_id_public_key
       << iic.initiator_signature
       << iic.user_data_in;
    return oa;
}

inline flurry::iarchive& operator >> (flurry::iarchive& ia, initiator_identity_chunk& iic)
{
    ia >> iic.initiator_channel_number
       >> iic.initiator_eid
       >> iic.responder_eid
       >> iic.initiator_id_public_key
       >> iic.initiator_signature
       >> iic.user_data_in;
    return ia;
}

//=================================================================================================
// responder_identity_chunk
//=================================================================================================

struct responder_identity_chunk
{
    channel_number responder_channel_number;
    byte_array     responder_eid;
    byte_array     responder_id_public_key;
    byte_array     responder_signature; // Responder's parameter signature
    byte_array     user_data_out; // Upper-level protocol data
};

//-------------------------------------------------------------------------------------------------

inline flurry::oarchive& operator << (flurry::oarchive& oa, responder_identity_chunk const& ric)
{
    oa << ric.responder_channel_number
       << ric.responder_eid
       << ric.responder_id_public_key
       << ric.responder_signature
       << ric.user_data_out;
    return oa;
}

inline flurry::iarchive& operator >> (flurry::iarchive& ia, responder_identity_chunk& ric)
{
    ia >> ric.responder_channel_number
       >> ric.responder_eid
       >> ric.responder_id_public_key
       >> ric.responder_signature
       >> ric.user_data_out;
    return ia;
}

//=================================================================================================
// DH/JFK negotiation chunks
//
// dh_init1_chunk
//=================================================================================================

struct dh_init1_chunk
{
    dh_group_type  group{dh_group_type::dh_group_1024};  // DH group of initiator's public key
    uint32_t       key_min_length{0};                    // Minimum AES key length: 16, 24, 32
    byte_array     initiator_hashed_nonce;               // Initiator's SHA256-hashed nonce
    byte_array     initiator_dh_public_key;              // Initiator's DH public key
    byte_array     responder_eid;                        // Optional: desired EID of responder
};

//-------------------------------------------------------------------------------------------------

inline flurry::oarchive& operator << (flurry::oarchive& oa, dh_init1_chunk const& dc1)
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

//=================================================================================================
// dh_response1_chunk
//=================================================================================================

struct dh_response1_chunk
{
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

//-------------------------------------------------------------------------------------------------

inline flurry::oarchive& operator << (flurry::oarchive& oa, dh_response1_chunk const& dc1)
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

//=================================================================================================
// dh_init2_chunk
//=================================================================================================

struct dh_init2_chunk
{
    dh_group_type  group;                       // DH group for public keys
    int            key_min_length;              // AES key length: 16, 24, or 32
    byte_array     initiator_nonce;             // Initiator's original nonce
    byte_array     responder_nonce;             // Responder's nonce
    byte_array     initiator_dh_public_key;     // Initiator's DH public key
    byte_array     responder_dh_public_key;     // Responder's DH public key
    byte_array     responder_challenge_cookie;  // Responder's challenge cookie
    byte_array     initiator_info;              // Initiator's encrypted identity
};

//-------------------------------------------------------------------------------------------------

inline flurry::oarchive& operator << (flurry::oarchive& oa, dh_init2_chunk const& dc2)
{
    oa << dc2.group
       << dc2.key_min_length
       << dc2.initiator_nonce
       << dc2.responder_nonce
       << dc2.initiator_dh_public_key
       << dc2.responder_dh_public_key
       << dc2.responder_challenge_cookie
       << dc2.initiator_info;
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
       >> dc2.initiator_info;
    return ia;
}

//=================================================================================================
// dh_response2_chunk
//=================================================================================================

struct dh_response2_chunk
{
    byte_array  initiator_hashed_nonce;         // Initiator's original nonce
    byte_array  responder_info;                 // Responder's encrypted identity
};

//-------------------------------------------------------------------------------------------------

inline flurry::oarchive& operator << (flurry::oarchive& oa, dh_response2_chunk const& dc2)
{
    oa << dc2.initiator_hashed_nonce
       << dc2.responder_info;
    return oa;
}

inline flurry::iarchive& operator >> (flurry::iarchive& ia, dh_response2_chunk& dc2)
{
    ia >> dc2.initiator_hashed_nonce
       >> dc2.responder_info;
    return ia;
}

//=================================================================================================
// key_chunk_type
//=================================================================================================

enum class key_chunk_type : uint32_t
{
    packet             = 0x0001,
    dh_init1           = 0x0021,
    dh_response1       = 0x0022,
    dh_init2           = 0x0023,
    dh_response2       = 0x0024,
};

//=================================================================================================
// key_chunk
//=================================================================================================

struct key_chunk
{
    key_chunk_type                             type;
    boost::optional<packet_chunk>              packet;
    boost::optional<dh_init1_chunk>            dh_init1;
    boost::optional<dh_response1_chunk>        dh_response1;
    boost::optional<dh_init2_chunk>            dh_init2;
    boost::optional<dh_response2_chunk>        dh_response2;
};

//-------------------------------------------------------------------------------------------------

inline flurry::oarchive& operator << (flurry::oarchive& oa, key_chunk const& kc)
{
    oa << kc.type;
    switch (kc.type) {
        case key_chunk_type::packet:
            oa << *kc.packet;
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

//=================================================================================================
// key_message
//=================================================================================================

struct key_message
{
    uint32_t magic;
    /**
     * Negotiate encryption, authentication and compression features for this session using
     * a list of strings in 'features' vector.
     * Features are listed in order of decreasing priority. First feature to match
     * on receiver side is one to use and is sent back in the response.
     */
    std::vector<std::string> features;
    std::vector<key_chunk> chunks;
};

//-------------------------------------------------------------------------------------------------

inline flurry::oarchive& operator << (flurry::oarchive& oa, key_message const& km)
{
    big_uint32_t magic_out(km.magic);
    oa.pack_raw_data(reinterpret_cast<const char*>(&magic_out), 4);
    oa << km.features << km.chunks;
    return oa;
}

inline flurry::iarchive& operator >> (flurry::iarchive& ia, key_message& km)
{
    byte_array ub;
    ub.resize(4);
    ia.unpack_raw_data(ub);
    km.magic = ub.as<big_uint32_t>()[0];
    ia >> km.features >> km.chunks;
    return ia;
}

} // negotiation namespace
} // ssu namespace
