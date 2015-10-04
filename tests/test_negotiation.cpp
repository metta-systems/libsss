//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2015, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#define BOOST_TEST_MODULE Test_negotiation
#include <boost/test/unit_test.hpp>

#include "simulator_fixture.h"

using namespace std;
using namespace sss;
using namespace sss::simulation;

BOOST_FIXTURE_TEST_CASE(negotiate_channel, simulator_fixture)
{
    // no routing in simulator yet
    bool hinted = client->add_location_hint(server_host_eid, server_host_address);
    BOOST_CHECK(hinted == true);
    client->connect_to(server_host_eid, "simulator", "test", server_host_address);

    simulator->run();

    logger::debug() << "<<< shutdown from this point on";
}
