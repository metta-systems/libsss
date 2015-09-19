//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#define BOOST_TEST_MODULE Test_sss_stream_user
#include <boost/test/unit_test.hpp>

#include "sss/host.h"
#include "sss/stream.h"

using namespace std;
using namespace sss;

BOOST_AUTO_TEST_CASE(created_stream)
{
    shared_ptr<host> h(host::create());
    stream s(h);
}

BOOST_AUTO_TEST_CASE(connect_to)
{
    uia::peer_identity eid;
    uia::comm::endpoint local_ep(boost::asio::ip::udp::v4(), stream_protocol::default_port);
    shared_ptr<host> h(host::create());
    stream s(h);
    // s.connect_to(eid, "test", "test", local_ep);
}
