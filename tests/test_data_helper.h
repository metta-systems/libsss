//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include "arsenal/byte_array.h"
#include "arsenal/byte_array_wrap.h"
#include "arsenal/flurry.h"
#include "sss/stream_protocol.h"
#include "sss/negotiation/key_message.h"

/**
 * Generate a binary blob for testing key_message.h I/O functions.
 * Create a key message with dh_init1 and checksum_init chunks, add packet chunk of data afterwards.
 */
inline byte_array
generate_dh1_chunk()
{
    byte_array data;
    {
        sss::negotiation::key_message m;
        sss::negotiation::key_chunk chu, chu2;
        sss::negotiation::dh_init1_chunk dh;
        sss::negotiation::packet_chunk pkt;

        m.magic = sss::stream_protocol::magic_id;
        chu.type = sss::negotiation::key_chunk_type::dh_init1;
        dh.group = sss::negotiation::dh_group_type::dh_group_1024;
        dh.key_min_length = 0x10;

        dh.initiator_hashed_nonce.resize(32);
        for (int i = 0; i < 32; ++i)
            dh.initiator_hashed_nonce[i] = i;
        dh.initiator_dh_public_key.resize(128);
        for (int i = 0; i < 128; ++i)
            dh.initiator_dh_public_key[i] = 255 - i;

        chu.dh_init1 = dh;

        m.chunks.push_back(chu);

        chu2.type = sss::negotiation::key_chunk_type::packet;
        pkt.data = {'H','e','l','l','o',' ','w','o','r','l','d','!'};
        chu2.packet = pkt;

        m.chunks.push_back(chu2);

        byte_array_owrap<flurry::oarchive> write(data);
        write.archive() << m;
    }
    return data;
}
