//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "simulation/sim_host.h"
#include "simulation/sim_timer_engine.h"
#include "simulation/simulator.h"
#include "simulation/sim_link.h"

namespace ssu {
namespace simulation {

boost::posix_time::ptime sim_host::current_time()
{
    return simulator_->current_time();
}

std::unique_ptr<async::timer_engine> sim_host::create_timer_engine_for(async::timer* t)
{
    return std::unique_ptr<async::timer_engine>(new sim_timer_engine(t, simulator_));
}

std::unique_ptr<link> sim_host::create_link()
{
    return std::unique_ptr<link>(new sim_link(std::static_pointer_cast<sim_host>(shared_from_this())));
}

void sim_host::enqueue_packet(sim_packet* packet)
{
    packet_queue.push(packet);
}

void sim_host::dequeue_packet(sim_packet* packet)
{
    // packet_queue.erase(packet);
    // removing from a priority_queue is non-trivial
}

void sim_host::register_connection_at(endpoint const& address, sim_connection* conn)
{
    assert(connections.find(address) == connections.end());
    connections.insert(std::make_pair(address, conn));
}

void sim_host::unregister_connection_at(endpoint const& address, sim_connection* conn)
{
    assert(connections.find(address) != connections.end());
    assert(connections.find(address)->second == conn);
    connections.erase(address);
}

} // simulation namespace
} // ssu namespace
