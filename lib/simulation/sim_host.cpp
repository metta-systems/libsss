//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "sss/simulation/sim_host.h"
#include "sss/simulation/sim_timer_engine.h"
#include "sss/simulation/simulator.h"
#include "sss/simulation/sim_link.h"
#include "sss/simulation/sim_packet.h"
#include "sss/simulation/sim_connection.h"
#include "arsenal/make_unique.h"
#include "arsenal/algorithm.h"

using namespace std;

namespace sss {
namespace simulation {

shared_ptr<sim_host>
sim_host::create(shared_ptr<simulator> sim)
{
    auto host = make_shared<sim_host>(sim);
    host->coordinator = make_shared<uia::routing::client_coordinator>(host); // @fixme LOOP
    // No need to call init_link here because primary link initialized and bound there
    // is not used anywhere! Calling init_link here causes errors because of real endpoint
    // binding attempts; check how it should be set up betterer and reenable here.
    // host->init_link(nullptr);
    return host;
}

sim_host::sim_host(shared_ptr<simulator> sim)
    : host()
    , simulator_(sim)
{}

sim_host::~sim_host()
{
    // Close all links.
    for (auto link : links_) {
        link.second->unbind();
    }

    // Disconnect from other hosts.
    for (auto conn : connections_) {
        conn.second->disconnect();
    }
    assert(connections_.empty());

    packet_queue_.clear();
}

boost::posix_time::ptime
sim_host::current_time()
{
    return simulator_->current_time();
}

unique_ptr<async::timer_engine>
sim_host::create_timer_engine_for(async::timer* t)
{
    return stdext::make_unique<sim_timer_engine>(t, simulator_);
}

shared_ptr<uia::comm::socket>
sim_host::create_socket()
{
    return make_shared<sim_link>(static_pointer_cast<sim_host>(shared_from_this()));
}

void
sim_host::enqueue_packet(shared_ptr<sim_packet> packet)
{
    // @todo replace with std::upper_bound()?
    size_t i = 0;
    for (; i < packet_queue_.size(); ++i)
    {
        if (packet->arrival_time() < packet_queue_[i]->arrival_time()) {
            break;
        }
    }
    packet_queue_.insert(packet_queue_.begin() + i, packet);
}

void
sim_host::dequeue_packet(shared_ptr<sim_packet> packet)
{
    // @todo Replace with .erase(packet)?
    for (auto it = find(packet_queue_.begin(), packet_queue_.end(), packet); it != packet_queue_.end();)
    {
        packet_queue_.erase(it);
        it = find(packet_queue_.begin(), packet_queue_.end(), packet);
    }
}

bool
sim_host::packet_on_queue(shared_ptr<sim_packet> packet) const
{
    return find(packet_queue_.begin(), packet_queue_.end(), packet) != packet_queue_.end();
}

void
sim_host::register_connection_at(uia::comm::endpoint const& address,
    shared_ptr<sim_connection> conn)
{
    assert(!contains(connections_, address));
    connections_.insert(std::make_pair(address, conn));
}

void
sim_host::unregister_connection_at(uia::comm::endpoint const& address,
    shared_ptr<sim_connection> conn)
{
    assert(contains(connections_, address));
    assert(connections_.find(address)->second == conn);
    connections_.erase(address);
}

shared_ptr<sim_connection>
sim_host::connection_at(uia::comm::endpoint const& ep)
{
    return connections_[ep];
}

shared_ptr<sim_host>
sim_host::neighbor_at(uia::comm::endpoint const& dst, uia::comm::endpoint& src)
{
    for (auto conn : connections_)
    {
        shared_ptr<sim_host> uplink =
            conn.second->uplink_for(static_pointer_cast<sim_host>(shared_from_this()));
        if (conn.second->address_for(uplink) == dst)
        {
            src = conn.first;
            return uplink;
        }
    }
    return nullptr;
}

void
sim_host::register_link_at(uint16_t port, std::shared_ptr<sim_link> link)
{
    assert(links_[port] == nullptr);
    links_[port] = link;
}

void
sim_host::unregister_link_at(uint16_t port, std::shared_ptr<sim_link> link)
{
    assert(links_[port] == link);
    links_.erase(port);
}

shared_ptr<sim_link>
sim_host::link_for(uint16_t port)
{
    return links_[port];
}

vector<uia::comm::endpoint>
sim_host::local_endpoints()
{
    vector<uia::comm::endpoint> eps;
    for (auto v : connections_) {
        eps.push_back(v.first);
    }
    return eps;
}

} // simulation namespace
} // sss namespace
