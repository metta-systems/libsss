//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include <fstream>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/optional/optional.hpp>
#include "protocol.h"
#include "byte_array.h"
#include "custom_optional.h"
#include "msgpack.h"
#include "negotiation/key_message.h"
#define BOOST_TEST_MODULE Test_key_message_serialization
#include <boost/test/unit_test.hpp>

#include "link.h"
#include "test_data_helper.h"

using namespace std;

BOOST_AUTO_TEST_CASE(serialize_and_deserialize)
{
    byte_array data = generate_dh1_chunk();

    {
        logger::file_dump out(data);
    }

    {
        byte_array_iwrap<boost::archive::binary_iarchive> r(data);
        ssu::negotiation::key_message m;

        BOOST_CHECK(data.size() == 188);

        r.archive() >> m;

        BOOST_CHECK(m.magic == ssu::stream_protocol::magic);
        BOOST_CHECK(m.chunks.size() == 1);
        BOOST_CHECK(m.chunks[0].type == ssu::negotiation::key_chunk_type::dh_init1);
        BOOST_CHECK(m.chunks[0].dh_init1.is_initialized());
        BOOST_CHECK(m.chunks[0].dh_init1->group == ssu::negotiation::dh_group_type::dh_group_1024);
        BOOST_CHECK(m.chunks[0].dh_init1->key_min_length = 0x10);
        BOOST_CHECK(m.chunks[0].dh_init1->initiator_hashed_nonce.size() == 32);
        for (int i = 0; i < 128; ++i) {
            BOOST_CHECK(uint8_t(m.chunks[0].dh_init1->initiator_dh_public_key[i]) == 255 - i);
        }
    }
}
