//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#define BOOST_TEST_MODULE Test_ssu_stream_user
#include <boost/test/unit_test.hpp>

#include "host.h"
#include "stream.h"

using namespace std;
using namespace ssu;

BOOST_AUTO_TEST_CASE(created_stream)
{
    shared_ptr<host> h(host::create());
    stream s(h);
}

BOOST_AUTO_TEST_CASE(connect_to)
{
    peer_id eid;
    ssu::endpoint local_ep(boost::asio::ip::udp::v4(), stream_protocol::default_port);
    shared_ptr<host> h(host::create());
    stream s(h);
    // s.connect_to(eid, "test", "test", local_ep);
}
