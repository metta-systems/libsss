//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2015, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include "sodiumpp/sodiumpp.h"
#include "arsenal/fusionary.hpp"

// TODO: BIG ENDIAN!

using usid_t     = std::array<uint8_t, 24>;
using eckey_t    = std::array<uint8_t, 32>;
using cnonce8_t  = std::array<uint8_t, 8>;
using cnonce16_t = std::array<uint8_t, 16>;
using box48_t    = std::array<uint8_t, 48>;
using box64_t    = std::array<uint8_t, 64>;
using box80_t    = std::array<uint8_t, 80>;
using box96_t    = std::array<uint8_t, 96>;
using box144_t   = std::array<uint8_t, 144>;
using nonce64    = sodiumpp::nonce<crypto_box_NONCEBYTES - 8, 8>;
using nonce128   = sodiumpp::nonce<crypto_box_NONCEBYTES - 16, 16>;
using recv_nonce = sodiumpp::source_nonce<crypto_box_NONCEBYTES>;

template <size_t N>
std::array<uint8_t, N>
as_array(std::string const& s)
{
    assert(s.size() == N);
    std::array<uint8_t, N> ret;
    std::copy(s.begin(), s.end(), ret.begin());
    return ret;
}

template <size_t N>
std::string
as_string(std::array<uint8_t, N> const& a)
{
    std::string ret;
    ret.resize(N);
    std::copy(a.begin(), a.end(), ret.begin());
    return ret;
}

inline std::string
as_string(rest_t const& a)
{
    return a.data;
}

namespace magic {
using hello_packet    = std::integral_constant<uint64_t, 0x71564e7135784c68>; // "qVNq5xLh"
using cookie_packet   = std::integral_constant<uint64_t, 0x726c33416e6d786b>; // "rl3Anmxk"
using initiate_packet = std::integral_constant<uint64_t, 0x71564e7135784c69>; // "qVNq5xLi"
using message_packet  = std::integral_constant<uint64_t, 0x726c337135784c6d>; // "rl3q5xLm"
}

const std::string HELLO_NONCE_PREFIX     = "cURVEcp-CLIENT-h";
const std::string MINUTEKEY_NONCE_PREFIX = "minute-k";
const std::string COOKIE_NONCE_PREFIX    = "cURVEcpk";
const std::string VOUCH_NONCE_PREFIX     = "cURVEcpv";
const std::string INITIATE_NONCE_PREFIX  = "cURVEcp-CLIENT-i";
const std::string MESSAGE_NONCE_PREFIX   = "cURVEcp-CLIENT-m";
