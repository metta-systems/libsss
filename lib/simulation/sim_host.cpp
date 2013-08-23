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
#include "simulation/sim_packet.h"
#include "simulation/sim_connection.h"

namespace ssu {
namespace simulation {

sim_host::sim_host(std::shared_ptr<simulator> sim)
    : simulator_(sim)
{}

sim_host::~sim_host()
{}

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
    packet_queue_.push(packet);
}

void sim_host::dequeue_packet(sim_packet* packet)
{
    // packet_queue_.erase(packet);
    // removing from a priority_queue is non-trivial
    delete packet;
}

bool sim_host::packet_on_queue(sim_packet* packet) const
{
    return false;
}

void
sim_host::register_connection_at(endpoint const& address, std::shared_ptr<sim_connection> conn)
{
    assert(connections_.find(address) == connections_.end());
    connections_.insert(std::make_pair(address, conn));
}

void
sim_host::unregister_connection_at(endpoint const& address, std::shared_ptr<sim_connection> conn)
{
    assert(connections_.find(address) != connections_.end());
    assert(connections_.find(address)->second == conn);
    connections_.erase(address);
}

std::shared_ptr<sim_connection>
sim_host::connection_at(endpoint const& ep)
{
    return connections_[ep];
}

std::shared_ptr<sim_host>
sim_host::neighbor_at(endpoint const& dst, endpoint& src)
{
    for (auto conn : connections_)
    {
        std::shared_ptr<sim_host> uplink = conn.second->uplink_for(std::static_pointer_cast<sim_host>(shared_from_this()));
        if (conn.second->address_for(uplink) == dst)
        {
            src = conn.first;
            return uplink;
        }
    }
    return nullptr;
}

std::shared_ptr<sim_link>
sim_host::link_for(uint16_t port)
{
    return links_[port];
}

std::vector<endpoint>
sim_host::local_endpoints()
{
    std::vector<endpoint> eps;
    for (auto v : connections_) {
        eps.push_back(v.first);
    }
    return eps;
}

} // simulation namespace
} // ssu namespace
