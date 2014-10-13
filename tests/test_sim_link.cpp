//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#define BOOST_TEST_MODULE Test_sim_link
#include <boost/test/unit_test.hpp>

#include "sss/simulation/simulator.h"
#include "sss/simulation/sim_host.h"
#include "sss/simulation/sim_link.h"
#include "sss/simulation/sim_connection.h"

using namespace std;
using namespace uia;
using namespace sss;
using namespace sss::simulation;

BOOST_AUTO_TEST_CASE(created_link)
{
    shared_ptr<simulator> sim(make_shared<simulator>());
    shared_ptr<sim_host> my_host(sim_host::create(sim));

    shared_ptr<comm::socket> link = my_host->create_socket();
    BOOST_CHECK(link != nullptr);
}

BOOST_AUTO_TEST_CASE(connected_link)
{
    shared_ptr<simulator> sim(make_shared<simulator>());
    shared_ptr<sim_host> my_host(sim_host::create(sim));
    shared_ptr<sim_host> other_host(sim_host::create(sim));

    shared_ptr<sim_connection> conn = make_shared<sim_connection>();
    conn->connect(other_host, comm::endpoint(boost::asio::ip::address_v4::from_string("10.0.0.2"),0),
                  my_host, comm::endpoint(boost::asio::ip::address_v4::from_string("10.0.0.1"),0));

    shared_ptr<comm::socket> link = my_host->create_socket();
    BOOST_CHECK(link->local_endpoints().size() == 1);
}
