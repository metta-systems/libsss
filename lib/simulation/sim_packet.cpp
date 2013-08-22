//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_real.hpp>
#include "simulation/simulator.h"
#include "simulation/sim_packet.h"
#include "simulation/sim_host.h"
#include "simulation/sim_connection.h"
#include "logging.h"

namespace ssu {
namespace simulation {

static const int packet_overhead = 32; // Bytes of link/inet overhead per packet

sim_packet::sim_packet(std::shared_ptr<sim_host> source_host, endpoint const& src,
                       std::shared_ptr<sim_connection> pipe, endpoint const& dst,
                       byte_array data)
    : simulator_(source_host->get_simulator())
    , from_(src)
    , to_(dst)
    , data_(data)
    , target_host_(nullptr)
    , timer_(source_host.get())
{
    target_host_ = pipe->find_uplink(source_host);
    if (!target_host_) {
        logger::warning() << "Destination host " << dst << " not found on link " << pipe;
        return; // @todo - this packet should clean up itself somehow
    }

    boost::posix_time::ptime now = simulator_->current_time();

    // Get other side's params
    sim_connection::params param = pipe->params_for(target_host_);
    boost::posix_time::ptime& arrival_time = pipe->arrival_time_for(target_host_);

    static boost::random::mt19937 rng;
    boost::uniform_real<> uni_dist(0,1);

    // Simulate random loss
    if (param.loss > 0.0 and uni_dist(rng) <= param.loss)
    {
        logger::info() << "Packet DROPPED";
        return; // @todo - this packet should clean up itself somehow
    }

    // Earliest time packet could start to arrive based on network delay
    boost::posix_time::ptime nominal_arrival = now + param.delay;

    // Compute the time the packet's first bit will actually arrive -
    // it can't start arriving sooner than the last packet finished.
    boost::posix_time::ptime actual_arrival = std::max(nominal_arrival, arrival_time);

    // If the computed arrival time is too late, drop this packet.
    // Implement a standard, basic drop-tail policy.
    bool drop = actual_arrival > nominal_arrival + param.queue;

    // Compute the amount of wire time this packet takes to transmit,
    // including some per-packet link/inet overhead
    int64_t packet_size = data.size() + packet_overhead;
    async::timer::duration_type packet_time =
        boost::posix_time::microseconds(packet_size * 1000000 / param.rate);

    if (drop)
    {
        logger::info() << "Packet DROPPED";
        return; // @todo - this packet should clean up itself somehow
    }

    // Finally, record the time the packet will finish arriving,
    // and schedule the packet to arrive at that time.
    arrival_time = actual_arrival + packet_time; // Updates connection's actual arrival time.

    target_host_->enqueue_packet(this);

    timer_.on_timeout.connect(boost::bind(&sim_packet::arrive, this));
    timer_.start(arrival_time - now);
}

sim_packet::~sim_packet()
{
    if (target_host_) {
        target_host_->dequeue_packet(this);
    }
}

void sim_packet::arrive()
{}

} // simulation namespace
} // ssu namespace
