//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#define BOOST_TEST_MODULE Test_simulator
#include <boost/test/unit_test.hpp>

#include "simulator_fixture.h"

using namespace std;
using namespace ssu;
using namespace ssu::simulation;

BOOST_AUTO_TEST_CASE(created_simulator)
{
    shared_ptr<simulator> sim(make_shared<simulator>());
    BOOST_CHECK(sim != nullptr);
}

BOOST_FIXTURE_TEST_SUITE(simulator, simulator_fixture)

BOOST_AUTO_TEST_CASE(simple_sim_step)
{
    // no routing in simulator yet
    bool hinted = client->add_location_hint(server_host_eid, server_host_address);
    BOOST_CHECK(hinted == true);
    client->connect_to(server_host_eid, "simulator", "test", server_host_address);

    // client->write_data("test1", 6);

    the_simulator->run();

    logger::debug() << "<<< shutdown from this point on";
}

BOOST_AUTO_TEST_CASE(connect_wrong_service)
{
    // Connect to wrong service and protocol here.
    client->connect_to(server_host_eid, "test", "simulator", server_host_address);

    the_simulator->run();

    logger::debug() << "<<< shutdown from this point on";

    logger::debug() << "<<< host use counts " << dec << client_host.use_count()
        << " and " << server_host.use_count();
}

BOOST_AUTO_TEST_SUITE_END()
