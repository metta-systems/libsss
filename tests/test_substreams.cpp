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

BOOST_FIXTURE_TEST_CASE(one_substream, simulator_fixture)
{
    simulator->run();
}

// Test stream with one, two and three levels on substreams.
//
// stream                         sends nothing then QUIT! when done
// +--substream                   sends ONE!
// +--substream                   sends TWO!
//    +--subsubstream             sends TWO.ONE!
// +--substream                   sends THREE!
//    +--subsubstream             sends THREE.ONE!
//       +--subsubsubstream       sends THREE.ONE.TWO!

BOOST_FIXTURE_TEST_CASE(multiple_substreams, simulator_fixture)
{
    // volatile bool running = true;
    // send_text(stream s, std::string t) { while(running) strand_.post([this](s) { s->write_record(t); })}
    // std::thread([this](std::string text) {send_text(text)});
    // std::this_thread::sleep_for(chrono::seconds(10));
    // running = false;
    // all_threads.join();

    client->connect_to(server_host_eid, "simulator", "test", server_host_address);

    auto substream1 = client->open_substream();
    substream1->write_record("ONE!");
    auto substream2 = client->open_substream();
    substream2->write_record("TWO!");
    auto substream21 = substream2->open_substream();
    substream21->write_record("TWO.ONE!");
    auto substream3 = client->open_substream();
    substream3->write_record("THREE!");
    auto substream31 = substream3->open_substream();
    substream31->write_record("THREE.ONE!");
    auto substream312 = substream31->open_substream();
    substream312->write_record("THREE.ONE.TWO!");

    simulator->run();
}
