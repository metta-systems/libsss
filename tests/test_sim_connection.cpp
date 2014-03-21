//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#define BOOST_TEST_MODULE Test_sim_connection
#include <boost/test/unit_test.hpp>

#include "ssu/simulation/simulator.h"
#include "ssu/simulation/sim_host.h"
#include "ssu/simulation/sim_link.h"
#include "ssu/simulation/sim_connection.h"

using namespace std;
using namespace uia;
using namespace ssu;
using namespace ssu::simulation;

BOOST_AUTO_TEST_CASE(created_connection)
{
    shared_ptr<sim_connection> conn = make_shared<sim_connection>();
    BOOST_CHECK(conn != nullptr);
}

BOOST_AUTO_TEST_CASE(connection_sides_correct)
{
    shared_ptr<simulator> sim(make_shared<simulator>());
    shared_ptr<sim_host> one_host(make_shared<sim_host>(sim));
    shared_ptr<sim_host> another_host(make_shared<sim_host>(sim));

    shared_ptr<sim_connection> conn = make_shared<sim_connection>();
    conn->connect(another_host, comm::endpoint(boost::asio::ip::address_v4::from_string("10.0.0.2"),0),
                  one_host, comm::endpoint(boost::asio::ip::address_v4::from_string("10.0.0.1"),0));

    BOOST_CHECK(conn->uplink_for(one_host) == another_host);
    BOOST_CHECK(conn->uplink_for(another_host) == one_host);
}
