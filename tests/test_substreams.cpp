//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#define BOOST_TEST_MODULE Test_ssu_substreams
#include <boost/test/unit_test.hpp>

#include "simulator_fixture.h"

using namespace std;
using namespace ssu;

struct substreams_fixture : public simulator_fixture
{
    std::vector<std::shared_ptr<ssu::stream>> streams; // to keep streams alive
    std::vector<byte_array> received;

    void stream_setup_listening(std::shared_ptr<ssu::stream> new_stream)
    {
        new_stream->set_child_receive_buffer_size(16384);
        new_stream->listen(ssu::stream::buffer_limit);
        new_stream->on_new_substream.connect([this, new_stream] {
            while (auto new_substream = new_stream->accept_substream()) {
                new_substream->on_ready_read_record.connect([this, new_substream] {
                    received.emplace_back(new_substream->read_record());
                });
                stream_setup_listening(new_substream);
            }
        });
        streams.emplace_back(new_stream);
    }
};

// Test stream with one, two and three levels on substreams.
//
// stream                         sends nothing
// +--substream                   sends ONE!

BOOST_FIXTURE_TEST_CASE(one_substream, substreams_fixture)
{
    server->on_new_connection.connect([&] {
        auto new_stream = server->accept(); // First incoming stream from client
        stream_setup_listening(new_stream);
    });

    client->connect_to(server_host_eid, "simulator", "test", server_host_address);

    auto substream1 = client->open_substream();
    substream1->write_record("ONE!"); // Should arrive last!

    simulator->run();

    BOOST_CHECK(received.size() == 1);
    BOOST_CHECK(received[0].as_string() == "ONE!");
}

// Test stream with one, two and three levels on substreams.
//
// stream                         sends nothing
// +--substream                   sends ONE!
// +--substream                   sends TWO!
//    +--subsubstream             sends TWO.ONE!
// +--substream                   sends THREE!
//    +--subsubstream             sends THREE.ONE!
//       +--subsubsubstream       sends THREE.ONE.TWO!

BOOST_FIXTURE_TEST_CASE(multiple_substreams, substreams_fixture)
{
    // volatile bool running = true;
    // send_text(stream s, std::string t) { while(running) strand_.post([this](s) { s->write_record(t); })}
    // std::thread([this](std::string text) {send_text(text)});
    // std::this_thread::sleep_for(chrono::seconds(10));
    // running = false;
    // all_threads.join();

    server->on_new_connection.connect([&] {
        auto new_stream = server->accept(); // First incoming stream from client
        stream_setup_listening(new_stream);
    });

    client->connect_to(server_host_eid, "simulator", "test", server_host_address);

    auto substream1 = client->open_substream();
    substream1->set_priority(1); // lowest
    auto substream2 = client->open_substream();
    substream2->set_priority(2);
    auto substream21 = substream2->open_substream();
    substream21->set_priority(4);
    auto substream3 = client->open_substream();
    substream1->set_priority(3);
    auto substream31 = substream3->open_substream();
    substream31->set_priority(5);
    auto substream312 = substream31->open_substream();
    substream312->set_priority(6); // highest

    substream1->write_record("ONE!"); // Should arrive last!
    substream2->write_record("TWO!");
    substream21->write_record("TWO.ONE!");
    substream3->write_record("THREE!");
    substream31->write_record("THREE.ONE!");
    substream312->write_record("THREE.ONE.TWO!"); // Should arrive first!

    simulator->run();

    for (auto b : received) {
        logger::debug() << b.as_string();
    }

    BOOST_CHECK(received.size() == 6);
    BOOST_CHECK(received[0].as_string() == "THREE.ONE.TWO!");
    BOOST_CHECK(received[1].as_string() == "THREE.ONE!");
    BOOST_CHECK(received[2].as_string() == "THREE!");
    BOOST_CHECK(received[3].as_string() == "TWO.ONE!");
    BOOST_CHECK(received[4].as_string() == "TWO!");
    BOOST_CHECK(received[5].as_string() == "ONE!");
}

