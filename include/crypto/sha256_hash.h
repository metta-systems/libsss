//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include "crypto.h"
#include "byte_array.h"

namespace ssu {
namespace crypto {

// A quick simple wrapper for instantly getting a sha-256 of a byte_array.
class sha256
{
public:
    static crypto::hash::value hash(char const* data, size_t size);
    static crypto::hash::value hash(byte_array const& data);
    static crypto::hash::value keyed_hash(byte_array const& key, byte_array const& data); // HMAC
};

inline crypto::hash::value sha256::hash(char const* data, size_t size)
{
    crypto::hash md;
    crypto::hash::value sha256hash;
    md.update(boost::asio::buffer(data, size));
    md.finalize(sha256hash);
    return sha256hash;
}

inline crypto::hash::value sha256::hash(byte_array const& data)
{
    crypto::hash md;
    crypto::hash::value sha256hash;
    md.update(data.as_vector());
    md.finalize(sha256hash);
    return sha256hash;
}

inline crypto::hash::value sha256::keyed_hash(byte_array const& key, byte_array const& data) // HMAC
{
    assert(key.size() == crypto::HMACKEYLEN);
    crypto::hash kmd(key.as_vector());
    crypto::hash::value mac;
    assert(mac.size() == crypto::HMACLEN);//mmmhm, expected HMACLEN is 16 but we generate 32 bytes HMACs... incompat?
    kmd.update(data.as_vector());
    kmd.finalize(mac);
    return mac;
}

} // crypto namespace
} // ssu namespace
