//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <iostream>
#include "arsenal/logging.h"
#include "sss/stream.h"
#include "sss/server.h"
#include "sss/simulation/simulator.h"
#include "sss/simulation/sim_host.h"
#include "sss/simulation/sim_socket.h"
#include "sss/simulation/sim_connection.h"

struct simulator_fixture
{
    std::shared_ptr<sss::simulation::simulator> simulator;
    std::shared_ptr<sss::simulation::sim_connection> server_client_connection;

    std::shared_ptr<sss::simulation::sim_host> server_host;
    uia::peer_identity server_host_eid;
    uia::comm::endpoint server_host_address;
    std::shared_ptr<uia::comm::socket> server_link;
    std::shared_ptr<sss::server> server;

    std::shared_ptr<sss::simulation::sim_host> client_host;
    uia::peer_identity client_host_eid;
    uia::comm::endpoint client_host_address;
    std::shared_ptr<uia::comm::socket> client_link;
    std::shared_ptr<sss::stream> client;

    simulator_fixture()
    {
        simulator = std::make_shared<sss::simulation::simulator>();
        BOOST_CHECK(simulator != nullptr);

        setup_test_server();
        setup_test_client();
        setup_test_connection();
    }

    ~simulator_fixture()
    {
        server_client_connection.reset();
        client.reset();
        client_link.reset();
        client_host.reset();
        server.reset();
        server_link.reset();
        server_host.reset();
        simulator.reset();
        logger::debug() << "<<< host use counts after reset " << std::dec << client_host.use_count()
                        << " and " << server_host.use_count();
    }

    void setup_test_server()
    {
        server_host = sss::simulation::sim_host::create(simulator);
        BOOST_CHECK(server_host != nullptr);
        server_host_eid = server_host->host_identity().id();
        server_host_address =
            uia::comm::endpoint(boost::asio::ip::address_v4::from_string("10.0.0.2"),
                                sss::stream_protocol::default_port);

        server_link = server_host->create_socket();
        BOOST_CHECK(server_link != nullptr);
        server_link->bind(server_host_address);
        BOOST_CHECK(server_link->is_active());

        server = std::make_shared<sss::server>(server_host);
        BOOST_CHECK(server != nullptr);
        bool listening = server->listen("simulator", "Simulating", "test", "Test protocol");
        BOOST_CHECK(listening == true);
    }

    void setup_test_client()
    {
        client_host = sss::simulation::sim_host::create(simulator);
        BOOST_CHECK(client_host != nullptr);
        client_host_eid = client_host->host_identity().id();
        client_host_address =
            uia::comm::endpoint(boost::asio::ip::address_v4::from_string("10.0.0.1"),
                                sss::stream_protocol::default_port);

        client_link = client_host->create_socket();
        BOOST_CHECK(client_link != nullptr);
        client_link->bind(client_host_address);
        BOOST_CHECK(client_link->is_active());

        client = std::make_shared<sss::stream>(client_host);
        BOOST_CHECK(client != nullptr);
    }

    void setup_test_connection()
    {
        server_client_connection = std::make_shared<sss::simulation::sim_connection>();
        BOOST_CHECK(server_client_connection != nullptr);
        server_client_connection->connect(
            server_host, server_host_address, client_host, client_host_address);
    }
};
