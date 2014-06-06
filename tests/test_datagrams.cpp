//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@atta-metta.net>
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

#include "simulator_fixture.h"

using namespace std;
using namespace ssu;
using namespace ssu::simulation;

static constexpr int DATAGRAMS_TO_SEND = 100;
static constexpr int max_datagram_size_log2 = 20;   // Max dgram size: 2^20 = 1MB
static constexpr int max_datagram_size = 1 << max_datagram_size_log2;

BOOST_FIXTURE_TEST_CASE(transmit_datagrams, simulator_fixture)
{
    int n_datagrams_arrived{0};
    shared_ptr<ssu::stream> server_stream{nullptr};

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
            simulator->post([&] { client->on_link_up(); });
        }
    });

    client->connect_to(server_host_eid, "simulator", "test", server_host_address);

    simulator->run();

    logger::debug() << "Datagram test completed: " << n_datagrams_arrived
        << " of " << DATAGRAMS_TO_SEND << " datagrams delivered";

    BOOST_CHECK(n_datagrams_arrived >= DATAGRAMS_TO_SEND*90/100);
}

