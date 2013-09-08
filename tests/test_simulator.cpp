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

#include "test_data_helper.h"
#include "stream.h"
#include "server.h"
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
    BOOST_CHECK(my_host != nullptr);
    endpoint my_host_address(boost::asio::ip::address_v4::from_string("10.0.0.1"),DEFAULT_PORT);
    shared_ptr<sim_host> other_host(make_shared<sim_host>(sim));
    BOOST_CHECK(other_host != nullptr);
    endpoint other_host_address(boost::asio::ip::address_v4::from_string("10.0.0.2"),DEFAULT_PORT);

    negotiation::key_responder my_receiver(my_host);
    my_host->bind_receiver(stream_protocol::magic, &my_receiver);

    negotiation::key_responder other_receiver(other_host);
    other_host->bind_receiver(stream_protocol::magic, &other_receiver);

    shared_ptr<sim_connection> conn = make_shared<sim_connection>();
    BOOST_CHECK(conn != nullptr);
    conn->connect(other_host, other_host_address,
                  my_host, my_host_address);

    shared_ptr<ssu::link> link = my_host->create_link();
    BOOST_CHECK(link != nullptr);
    link->bind(my_host_address);
    BOOST_CHECK(link->is_active());

    shared_ptr<ssu::link> other_link = other_host->create_link();
    BOOST_CHECK(other_link != nullptr);
    other_link->bind(other_host_address);
    BOOST_CHECK(other_link->is_active());

    shared_ptr<ssu::server> other_server(make_shared<ssu::server>(other_host));
    BOOST_CHECK(other_server != nullptr);
    bool res = other_server->listen("simulator", "Simulating", "test", "Test protocol");
    BOOST_CHECK(res == true);

    shared_ptr<ssu::stream> my_stream(make_shared<stream>(my_host));
    BOOST_CHECK(my_stream != nullptr);
    my_stream->connect_to(other_host->host_identity().id(), "simulator", "test", other_host_address);

    sim->run();
}
