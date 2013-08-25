//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#define BOOST_TEST_MODULE Test_simulator
#include <boost/test/unit_test.hpp>

#include "simulation/simulator.h"
#include "simulation/sim_host.h"
#include "simulation/sim_link.h"
#include "simulation/sim_connection.h"

using namespace std;
using namespace ssu;
using namespace ssu::simulation;

#define DEFAULT_PORT 9669

BOOST_AUTO_TEST_CASE(created_simulator)
{
    shared_ptr<simulator> sim(make_shared<simulator>());
    BOOST_CHECK(sim != nullptr);
}

BOOST_AUTO_TEST_CASE(simple_sim_step)
{
    shared_ptr<simulator> sim(make_shared<simulator>());
    BOOST_CHECK(sim != nullptr);

    shared_ptr<sim_host> my_host(make_shared<sim_host>(sim));
    endpoint my_host_address = endpoint(boost::asio::ip::address_v4::from_string("10.0.0.1"),DEFAULT_PORT);
    shared_ptr<sim_host> other_host(make_shared<sim_host>(sim));
    endpoint other_host_address = endpoint(boost::asio::ip::address_v4::from_string("10.0.0.2"),DEFAULT_PORT);

    shared_ptr<sim_connection> conn = make_shared<sim_connection>();
    conn->connect(other_host, other_host_address,
                  my_host, my_host_address);

    shared_ptr<ssu::link> link = my_host->create_link();
    link->bind(my_host_address);

    shared_ptr<ssu::link> other_link = other_host->create_link();
    other_link->bind(other_host_address);

    byte_array msg({'a', 'b', 'c'});

    link->send(other_host_address, msg);

    sim->run();
}
