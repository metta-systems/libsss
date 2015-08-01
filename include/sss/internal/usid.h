//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2015, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <cstdint>
#include <type_traits>
#include "arsenal/byte_array.h"
#include "arsenal/flurry.h"
#include "arsenal/hash_combine.h"

namespace sss {

using counter_t = uint64_t;    ///< Counter for SID assignment.
using local_stream_id_t = uint16_t;  ///< Stream ID within channel.

/**
 * Type for identifying streams uniquely across channels.
 */
struct unique_stream_id_t
{
    counter_t counter_{0}; ///< Stream counter in channel
    byte_array half_channel_id_; ///< Unique channel+direction ID ("half-channel id")

    inline unique_stream_id_t() = default;

    inline unique_stream_id_t(counter_t counter, byte_array chan_id)
        : counter_(counter)
        , half_channel_id_(chan_id)
    {}

    inline bool is_empty() const {
        return half_channel_id_.is_empty();
    }

    inline bool operator ==(unique_stream_id_t const& other) const {
        return counter_ == other.counter_ and half_channel_id_ == other.half_channel_id_;
    }
};

inline
std::ostream& operator << (std::ostream& os, unique_stream_id_t const& id)
{
    os << "USID[" << id.counter_ << ":" << id.half_channel_id_ << "]";
    return os;
}

inline
flurry::oarchive& operator << (flurry::oarchive& oa, unique_stream_id_t const& id)
{
    oa << id.counter_ << id.half_channel_id_;
    return oa;
}

inline
flurry::iarchive& operator >> (flurry::iarchive& ia, unique_stream_id_t& id)
{
    ia >> id.counter_ >> id.half_channel_id_;
    return ia;
}

} // sss namespace

// Hash specialization for unique_stream_id_t
namespace std {

template<>
struct hash<sss::unique_stream_id_t>
    : public std::unary_function<sss::unique_stream_id_t, size_t>
{
    inline size_t operator()(sss::unique_stream_id_t const& a) const noexcept
    {
        // VEEERY bad implementation for now. @fixme
        size_t seed = 0xdeafba1d;
        stdext::hash_combine(seed, a.counter_);
        stdext::hash_combine(seed, a.half_channel_id_);
        return seed;
    }
};

} // std namespace
