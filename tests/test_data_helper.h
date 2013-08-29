#pragma once

#include "msgpack_archive.h"
#include "archive_helper.h"
#include "byte_array.h"
#include "protocol.h"
#include "negotiation/key_message.h"

/**
 * Generate a binary blob for testing key_message.h I/O functions.
 * Create a key message with dh_init1 and checksum_init chunks, add packet chunk of data afterwards.
 *
 * +-------+--------+-----------------------------------------------------------------+
 * | magic | count  | opaque                                                          |
 * +-------+--------+--------+-------+------------------------------------------------+
 *                  | length | discr |                                                |
 *                  +--------+-------+------------------------------------------------+
 *                     0xb4    0x21  | DHgroup | keymin | nhi | length | dhi | length |
 *                                   +---------+--------+-----+--------+-----+--------+
 *                                       0x1      0x10    32b
 */
inline byte_array
generate_dh1_chunk()
{
    byte_array data;
    {
        ssu::negotiation::key_message m;
        ssu::negotiation::key_chunk chu;
        ssu::negotiation::dh_init1_chunk dh;

        m.magic = ssu::stream_protocol::magic;
        chu.type = ssu::negotiation::key_chunk_type::dh_init1;
        dh.group = ssu::negotiation::dh_group_type::dh_group_1024;
        dh.key_min_length = 0x10;

        dh.initiator_hashed_nonce.resize(32);
        for (int i = 0; i < 32; ++i)
            dh.initiator_hashed_nonce[i] = rand();
        dh.initiator_dh_public_key.resize(128);
        for (int i = 0; i < 128; ++i)
            dh.initiator_dh_public_key[i] = 255 - i;

        chu.dh_init1 = dh;

        m.chunks.push_back(chu);

        byte_array_owrap<msgpack_oarchive> w(data);
        w.archive() << m;
    }
    return data;
}
