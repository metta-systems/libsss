//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#define BOOST_TEST_MODULE Test_ssu_link
#include <boost/test/unit_test.hpp>

#include "ssu/host.h"//@todo
#include "ssu/udp_socket.h"
#include "ssu/negotiation/key_message.h"
#include "ssu/negotiation/key_responder.h"
#include "arsenal/byte_array_wrap.h"
#include "arsenal/flurry.h"

using namespace std;
using namespace ssu;
using namespace uia;

BOOST_AUTO_TEST_CASE(receive_too_small_packet)
{
    shared_ptr<host> host(make_shared<host>());
    comm::endpoint local_ep(boost::asio::ip::udp::v4(), stream_protocol::default_port);
    shared_ptr<udp_socket> link(make_shared<udp_socket>(host));
    link->bind(local_ep);

    byte_array msg({'a', 'b', 'c'});

    link->send(local_ep, msg);
}

BOOST_AUTO_TEST_CASE(local_endpoints)
{
    shared_ptr<host> host(make_shared<host>());
    comm::endpoint local_ep(boost::asio::ip::udp::v4(), stream_protocol::default_port);
    shared_ptr<udp_socket> link(make_shared<udp_socket>(host));
    link->bind(local_ep);

    link->local_endpoints();
}

BOOST_AUTO_TEST_CASE(bound_link_is_active)
{
    shared_ptr<host> host(make_shared<host>());
    comm::endpoint local_ep(boost::asio::ip::udp::v4(), stream_protocol::default_port);
    shared_ptr<udp_socket> link(make_shared<udp_socket>(host));
    link->bind(local_ep);
    BOOST_CHECK(link->is_active() == true);
}

BOOST_AUTO_TEST_CASE(receive_and_log_key_message)
{
    shared_ptr<host> host(make_shared<host>());
    comm::endpoint local_ep(boost::asio::ip::udp::v4(), stream_protocol::default_port);
    shared_ptr<udp_socket> link(make_shared<udp_socket>(host));
    link->bind(local_ep);

    // Add key responder to link. - this is done automagically when server::listen()s

    // Render key message to buffer.
    byte_array msg;

    {
        negotiation::key_message m;
        negotiation::key_chunk k;
        negotiation::dh_init1_chunk dh;

        dh.group = negotiation::dh_group_type::dh_group_1024;
        dh.key_min_length = 0x10;

        dh.initiator_hashed_nonce.resize(32);
        for (int i = 0; i < 32; ++i)
            dh.initiator_hashed_nonce[i] = rand();
        dh.initiator_dh_public_key.resize(128);
        for (int i = 0; i < 128; ++i)
            dh.initiator_dh_public_key[i] = 255 - i;

        k.type = negotiation::key_chunk_type::dh_init1;
        k.dh_init1 = dh;

        m.magic = stream_protocol::magic_id;
        m.chunks.push_back(k);

        byte_array_owrap<flurry::oarchive> write(msg);
        write.archive() << m;
    }

    // and send it to ourselves.
    link->send(local_ep, msg);

    // link->unbind(); //XXX should be done in the key_responder's receive method...

    host->run_io_service();
}
