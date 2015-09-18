//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2015, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include "sss/framing/framing_types.h"

//=================================================================================================
// Channel layer - transmitted packets
//=================================================================================================

// clang-format off
BOOST_FUSION_DEFINE_STRUCT(
    (sss)(channels), responder_cookie,
    (cnonce16_t, nonce)
    (box80_t, box)
);
// clang-format on

inline std::string
as_string(sss::channels::responder_cookie const& a)
{
    return as_string(a.nonce) + as_string(a.box);
}

// clang-format off
BOOST_FUSION_DEFINE_STRUCT(
    (sss)(channels), hello_packet_header,
    (magic::hello_packet, magic)
    (eckey_t, initiator_shortterm_public_key)
    (box64_t, zeros)
    (cnonce8_t, nonce)
    (box80_t, box)
);

BOOST_FUSION_DEFINE_STRUCT(
    (sss)(channels), cookie_packet_header,
    (magic::cookie_packet, magic)
    (cnonce16_t, nonce)
    (box144_t, box)
);

BOOST_FUSION_DEFINE_STRUCT(
    (sss)(channels), initiate_packet_header,
    (magic::initiate_packet, magic)
    (eckey_t, initiator_shortterm_public_key)
    (sss::channels::responder_cookie, responder_cookie)
    (cnonce8_t, nonce)
    (rest_t, box) // variable size box -- see struct below
);

BOOST_FUSION_DEFINE_STRUCT(
    (sss)(channels), initiate_packet_box,
    (eckey_t, initiator_longterm_public_key)
    (cnonce16_t, vouch_nonce)
    (box48_t, vouch)
    (rest_t, box) // variable size data containing initial frames
);

BOOST_FUSION_DEFINE_STRUCT(
    (sss)(channels), message_packet_header,
    (magic::message_packet, magic)
    (eckey_t, shortterm_public_key)
    (cnonce8_t, nonce)
    (rest_t, box) // variable size box containing message
);
// clang-format on
