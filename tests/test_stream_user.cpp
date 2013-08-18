//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#define BOOST_TEST_MODULE Test_ssu_stream_user
#include <boost/test/unit_test.hpp>

#include "stream.h"

using namespace ssu;

BOOST_AUTO_TEST_CASE(created_stream)
{
    std::shared_ptr<host> h(std::make_shared<host>());
    stream s(h);
}

BOOST_AUTO_TEST_CASE(connect_to)
{
    peer_id eid;
    std::shared_ptr<host> h(std::make_shared<host>());
    stream s(h);
    s.connect_to(eid, "test", "test");
}
