//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <boost/optional/optional.hpp>
#include "arsenal/logging.h"
#include "arsenal/flurry.h"
#include "arsenal/underlying.h"
#include "arsenal/opaque_endian.h"
#include "sss/stream_protocol.h"

namespace sss {
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
// Client->Server
// init1
//
// Hello = short_pk + '0' + client.nonce + box('0', short_pk, server_pk)
//
// Hello packet:
// (C',0,Box[0'](C'->S))
// where C' is the client's short-term public key
// and S is the server's long-term public key
// and 0 is zero-padding
// and 0' is zero-padding
//=================================================================================================

struct kex_hello_chunk
{
    byte_array shortterm_pk;
    byte_array zeros;
    byte_array client_nonce;
    byte_array box;
};

//-------------------------------------------------------------------------------------------------

inline flurry::oarchive& operator << (flurry::oarchive& oa, kex_hello_chunk const& dc1)
{
    oa << dc1.shortterm_pk
       << dc1.zeros
       << dc1.client_nonce
       << dc1.box;
    return oa;
}

inline flurry::iarchive& operator >> (flurry::iarchive& ia, kex_hello_chunk& dc1)
{
    ia >> dc1.shortterm_pk
       >> dc1.zeros
       >> dc1.client_nonce
       >> dc1.box;
    return ia;
}

//=================================================================================================
// Server->Client
// init1_reply
//
// Cookie packet:
// (Box[S',K](S->C'))
// where S' is the server's short-term public key
// and K is a cookie
//=================================================================================================

struct kex_cookie_chunk
{
    byte_array     server_nonce;
    byte_array     box;
};

//-------------------------------------------------------------------------------------------------

inline flurry::oarchive& operator << (flurry::oarchive& oa, kex_cookie_chunk const& dc1)
{
    oa << dc1.server_nonce
       << dc1.box;
    return oa;
}

inline flurry::iarchive& operator >> (flurry::iarchive& ia, kex_cookie_chunk& dc1)
{
    ia >> dc1.server_nonce
       >> dc1.box;
    return ia;
}

//=================================================================================================
// Client->Server
// init2
//
// Initiate packet with Vouch subpacket:
// (C',K,Box[C,V,N,...](C'->S'))
// where C is the client's long-term public key
// and V=Box[C'](C->S)
// and N is the server's domain name
// and ... is a message
//=================================================================================================

struct kex_initiate_chunk
{
    byte_array client_shortterm_pk;
    byte_array server_cookie;
    byte_array client_nonce;
    byte_array box;
};

//-------------------------------------------------------------------------------------------------

inline flurry::oarchive& operator << (flurry::oarchive& oa, kex_initiate_chunk const& chunk)
{
    oa << chunk.client_shortterm_pk
       << chunk.server_cookie
       << chunk.client_nonce
       << chunk.box;
    return oa;
}

inline flurry::iarchive& operator >> (flurry::iarchive& ia, kex_initiate_chunk& chunk)
{
    ia >> chunk.client_shortterm_pk
       >> chunk.server_cookie
       >> chunk.client_nonce
       >> chunk.box;
    return ia;
}

//=================================================================================================
// kex_chunk_type
//=================================================================================================

enum class kex_chunk_type : uint32_t
{
    packet             = 0x0001,
    hello              = 0x0011,
    cookie             = 0x0012,
    initiate           = 0x0013,
};

//=================================================================================================
// kex_chunk
//=================================================================================================

struct kex_chunk
{
    kex_chunk_type                             type;
    boost::optional<packet_chunk>              packet;
    boost::optional<kex_hello_chunk>           hello;
    boost::optional<kex_cookie_chunk>          cookie;
    boost::optional<kex_initiate_chunk>        initiate;
};

//-------------------------------------------------------------------------------------------------

inline flurry::oarchive& operator << (flurry::oarchive& oa, kex_chunk const& kc)
{
    oa << kc.type;
    switch (kc.type) {
        case kex_chunk_type::packet:
            oa << *kc.packet;
            break;
        case kex_chunk_type::hello:
            oa << *kc.hello;
            break;
        case kex_chunk_type::cookie:
            oa << *kc.cookie;
            break;
        case kex_chunk_type::initiate:
            oa << *kc.initiate;
            break;
    }
    return oa;
}

inline flurry::iarchive& operator >> (flurry::iarchive& ia, kex_chunk& kc)
{
    uint32_t type;
    ia >> type;
    kc.type = kex_chunk_type(type);
    switch (kc.type) {
        case kex_chunk_type::packet:
        {
            kc.packet = packet_chunk();
            ia >> *kc.packet;
            break;
        }
        case kex_chunk_type::hello:
        {
            kc.hello = kex_hello_chunk();
            ia >> *kc.hello;
            break;
        }
        case kex_chunk_type::cookie:
        {
            kc.cookie = kex_cookie_chunk();
            ia >> *kc.cookie;
            break;
        }
        case kex_chunk_type::initiate:
        {
            kc.initiate = kex_initiate_chunk();
            ia >> *kc.initiate;
            break;
        }
    }
    return ia;
}

//=================================================================================================
// kex_message
//=================================================================================================

struct kex_message
{
    uia::comm::magic_t magic;
    /**
     * Negotiate encryption, authentication and compression features for this session using
     * a list of strings in 'features' vector.
     * Features are listed in order of decreasing priority. First feature to match
     * on receiver side is one to use and is sent back in the response.
     */
    std::vector<std::string> features;
    std::vector<kex_chunk> chunks;
};

//-------------------------------------------------------------------------------------------------

inline flurry::oarchive& operator << (flurry::oarchive& oa, kex_message const& km)
{
    big_uint32_t magic_out(km.magic);
    oa.pack_raw_data(reinterpret_cast<const char*>(&magic_out), 4);
    oa << km.features << km.chunks;
    return oa;
}

inline flurry::iarchive& operator >> (flurry::iarchive& ia, kex_message& km)
{
    byte_array ub;
    ub.resize(4);
    ia.unpack_raw_data(ub);
    km.magic = ub.as<big_uint32_t>()[0];
    ia >> km.features >> km.chunks;
    return ia;
}

} // negotiation namespace
} // sss namespace
