//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#define BOOST_TEST_MODULE Test_datagrams
#include <boost/test/unit_test.hpp>

/*
 * @todo Other things this should test:
 *  - queueing datagrams immediately without waiting for on_link_up()
 *  - sending datagrams on multiple streams at once,
 *    to make sure fragmented datagrams interleave properly.
 *  - response to different line error rates.
 */

#include "stream.h"
#include "server.h"
#include "simulation/simulator.h"
#include "simulation/sim_host.h"
#include "simulation/sim_link.h"
#include "simulation/sim_connection.h"

using namespace std;
using namespace ssu;
using namespace ssu::simulation;

static constexpr int DATAGRAMS_TO_SEND = 100;
static constexpr int max_datagram_size_log2 = 20;   // Max dgram size: 2^20 = 1MB
static constexpr int max_datagram_size = 1 << max_datagram_size_log2;

BOOST_AUTO_TEST_CASE(transmit_datagrams)
{
    shared_ptr<simulator> sim(make_shared<simulator>());
    BOOST_CHECK(sim != nullptr);

    shared_ptr<sim_host> client_host(sim_host::create(sim));
    BOOST_CHECK(client_host != nullptr);
    endpoint client_host_address(boost::asio::ip::address_v4::from_string("10.0.0.1"), stream_protocol::default_port);
    shared_ptr<sim_host> server_host(sim_host::create(sim));
    BOOST_CHECK(server_host != nullptr);
    endpoint server_host_address(boost::asio::ip::address_v4::from_string("10.0.0.2"), stream_protocol::default_port);

    shared_ptr<sim_connection> conn = make_shared<sim_connection>();
    BOOST_CHECK(conn != nullptr);
    conn->connect(server_host, server_host_address,
                  client_host, client_host_address);

    shared_ptr<ssu::link> link = client_host->create_link();
    BOOST_CHECK(link != nullptr);
    link->bind(client_host_address);
    BOOST_CHECK(link->is_active());

    shared_ptr<ssu::link> other_link = server_host->create_link();
    BOOST_CHECK(other_link != nullptr);
    other_link->bind(server_host_address);
    BOOST_CHECK(other_link->is_active());

    int n_datagrams_arrived{0};
    shared_ptr<ssu::stream> server_stream{nullptr};

    shared_ptr<ssu::server> server(make_shared<ssu::server>(server_host));
    BOOST_CHECK(server != nullptr);

    auto got_datagram = [&] {
        byte_array dg = server_stream->read_datagram();
        if (dg.is_empty()) {
            return;
        }
        logger::debug() << "Received datagram size " << dec << dg.size();
        n_datagrams_arrived++;
    };

    server->on_new_connection.connect([&] {
        assert(server_stream == nullptr);
        server_stream = server->accept();
        if (!server_stream) {
            return;
        }
        server_stream->set_child_receive_buffer_size(max_datagram_size);
        server_stream->listen(stream::buffer_limit);

        server_stream->on_ready_read_datagram.connect([&] { got_datagram(); });
        got_datagram();
    });

    bool listening = server->listen("regress", "Regression tests", "dgram", "Datagram protocol");
    BOOST_CHECK(listening == true);

    shared_ptr<ssu::stream> client(make_shared<stream>(client_host));
    BOOST_CHECK(client != nullptr);
    bool hinted = client->add_location_hint(server_host->host_identity().id(), server_host_address); // no routing yet
    BOOST_CHECK(hinted == true);

    // Fire off a bunch of datagrams
    client->on_link_up.connect([&] {
        static int log2 = 4; // Min dgram size: 2^4 = 16 bytes
        static int i = 0;
        if (i < DATAGRAMS_TO_SEND)
        {
            ++i;
            byte_array buf;
            buf.resize(1 << log2);
            client->write_datagram(buf, stream::datagram_type::non_reliable);
            if (++log2 > max_datagram_size_log2) {
                log2 = 4;
            }
            sim->post([&] { client->on_link_up(); });
        }
    });

    client->connect_to(server_host->host_identity().id(), "regress", "dgram", server_host_address);

    sim->run();

    logger::debug() << "Datagram test completed: " << n_datagrams_arrived
        << " of " << DATAGRAMS_TO_SEND << " datagrams delivered";

    BOOST_CHECK(n_datagrams_arrived >= DATAGRAMS_TO_SEND*90/100);
}

