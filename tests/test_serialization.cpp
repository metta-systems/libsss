//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#define BOOST_TEST_MODULE Test_key_message_serialization
#include <boost/test/unit_test.hpp>
#include <fstream>
#include "protocol.h"
#include "byte_array.h"
#include "byte_array_wrap.h"
#include "flurry.h"
#include "negotiation/key_message.h"
#include "link.h"
#include "test_data_helper.h"
#include "logging.h"

using namespace std;

BOOST_AUTO_TEST_CASE(serialize_msgpack_types)
{
    byte_array data;
    {
        byte_array_owrap<flurry::oarchive> write(data);
        write.archive() << true << false << 42 << 0xdeadbeefabba << byte_array({'a','b','c','d','e'});
    }
    logger::file_dump out(data);
}

BOOST_AUTO_TEST_CASE(serialize_and_deserialize)
{
    byte_array data = generate_dh1_chunk();

    {
        logger::file_dump out(data);
    }

    {
        ssu::negotiation::key_message m;
        byte_array_iwrap<flurry::iarchive> read(data);

        BOOST_CHECK(data.size() == 0xaf);

        read.archive() >> m;

        BOOST_CHECK(m.magic == ssu::stream_protocol::magic);
        BOOST_CHECK(m.chunks.size() == 2);
        BOOST_CHECK(m.chunks[0].type == ssu::negotiation::key_chunk_type::dh_init1);
        BOOST_CHECK(m.chunks[0].dh_init1.is_initialized());
        BOOST_CHECK(m.chunks[0].dh_init1->group == ssu::negotiation::dh_group_type::dh_group_1024);
        BOOST_CHECK(m.chunks[0].dh_init1->key_min_length = 0x10);

        BOOST_CHECK(m.chunks[0].dh_init1->initiator_hashed_nonce.size() == 32);
        BOOST_CHECK(m.chunks[0].dh_init1->initiator_dh_public_key.size() == 128);

        for (int i = 0; i < 32; ++i) {
            BOOST_CHECK(uint8_t(m.chunks[0].dh_init1->initiator_hashed_nonce[i]) == i);
        }

        for (int i = 0; i < 128; ++i) {
            BOOST_CHECK(uint8_t(m.chunks[0].dh_init1->initiator_dh_public_key[i]) == 255 - i);
        }
    }
}
