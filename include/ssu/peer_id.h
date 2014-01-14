//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include "byte_array.h"
#include "flurry.h"
#include "base32.h"

namespace ssu { // @todo make this uia::peer_id

/**
 * Peer ID - helper for keeping all peer-related ID conversions in one place.
 * Contains a binary peer identifier plus methods to convert it into a string representation.
 *
 * Peer ID is a cryptographic identifier for the peer endpoint, it contains the type and keying method
 * as well as public key fingerprint of the peer.
 *
 * @todo maybe merge this with identity somehow?
 */
class peer_id
{
    byte_array id_;

public:
    inline peer_id() = default;
    inline peer_id(std::string base32) : id_(encode::from_base32(base32)) {}
    inline peer_id(byte_array id) : id_(id) {}

    inline byte_array id() const { return id_; }
    inline size_t size() const { return id_.size(); }
    inline std::string to_string() const { return encode::to_base32(id_); }
    inline bool is_empty() const { return id_.is_empty(); }
};

inline bool operator == (const peer_id& a, const peer_id& b) { return a.id() == b.id(); }
inline bool operator != (const peer_id& a, const peer_id& b) { return a.id() != b.id(); }

inline std::ostream& operator << (std::ostream& os, peer_id const& id)
{
    return os << id.to_string();
}

} // ssu namespace

namespace flurry {

inline flurry::oarchive& operator << (flurry::oarchive& oa, ssu::peer_id const& id)
{
    oa << id.id();
    return oa;
}

inline flurry::iarchive& operator >> (flurry::iarchive& ia, ssu::peer_id& id)
{
    byte_array i;
    ia >> i;
    id = i;
    return ia;
}


} // flurry namespace

// Hash specialization for peer_id
namespace std {

template<>
struct hash<ssu::peer_id> : public std::unary_function<ssu::peer_id, size_t>
{
    inline size_t operator()(ssu::peer_id const& a) const noexcept
    {
        return std::hash<byte_array>()(a.id());
    }
};

} // std namespace
