//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <iostream>
#include "arsenal/logging.h"
#include "ssu/stream.h"
#include "ssu/server.h"
#include "ssu/simulation/simulator.h"
#include "ssu/simulation/sim_host.h"
#include "ssu/simulation/sim_link.h"
#include "ssu/simulation/sim_connection.h"

struct simulator_fixture
{
    std::shared_ptr<ssu::simulation::simulator> the_simulator;
    std::shared_ptr<ssu::simulation::sim_connection> server_client_connection;

    std::shared_ptr<ssu::simulation::sim_host> server_host;
    ssu::peer_id server_host_eid;
    ssu::endpoint server_host_address;
    std::shared_ptr<ssu::link> server_link;
    std::shared_ptr<ssu::server> server;

    std::shared_ptr<ssu::simulation::sim_host> client_host;
    ssu::peer_id client_host_eid;
    ssu::endpoint client_host_address;
    std::shared_ptr<ssu::link> client_link;
    std::shared_ptr<ssu::stream> client;

    simulator_fixture() {
        the_simulator = std::make_shared<ssu::simulation::simulator>();
        BOOST_CHECK(the_simulator != nullptr);

        setup_test_server();
        setup_test_client();
        setup_test_connection();
    }

    ~simulator_fixture() {
        server_client_connection.reset();
        client.reset();
        client_link.reset();
        client_host.reset();
        server.reset();
        server_link.reset();
        server_host.reset();
        the_simulator.reset();
        logger::debug() << "<<< host use counts after reset " << std::dec << client_host.use_count()
            << " and " << server_host.use_count();
    }

    void setup_test_server()
    {
        server_host = ssu::simulation::sim_host::create(the_simulator);
        BOOST_CHECK(server_host != nullptr);
        server_host_eid = server_host->host_identity().id();
        server_host_address = ssu::endpoint(boost::asio::ip::address_v4::from_string("10.0.0.2"),
                                            ssu::stream_protocol::default_port);

        server_link = server_host->create_link();
        BOOST_CHECK(server_link != nullptr);
        server_link->bind(server_host_address);
        BOOST_CHECK(server_link->is_active());

        server = std::make_shared<ssu::server>(server_host);
        BOOST_CHECK(server != nullptr);
        bool listening = server->listen("simulator", "Simulating", "test", "Test protocol");
        BOOST_CHECK(listening == true);
    }

    void setup_test_client()
    {
        client_host = ssu::simulation::sim_host::create(the_simulator);
        BOOST_CHECK(client_host != nullptr);
        client_host_eid = client_host->host_identity().id();
        client_host_address = ssu::endpoint(boost::asio::ip::address_v4::from_string("10.0.0.1"),
                                            ssu::stream_protocol::default_port);

        client_link = client_host->create_link();
        BOOST_CHECK(client_link != nullptr);
        client_link->bind(client_host_address);
        BOOST_CHECK(client_link->is_active());

        client = std::make_shared<ssu::stream>(client_host);
        BOOST_CHECK(client != nullptr);
    }

    void setup_test_connection()
    {
        server_client_connection = std::make_shared<ssu::simulation::sim_connection>();
        BOOST_CHECK(server_client_connection != nullptr);
        server_client_connection->connect(server_host, server_host_address,
                                          client_host, client_host_address);
    }
};

