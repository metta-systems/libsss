//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "simulation/sim_packet.h"
#include "simulation/sim_host.h"
#include "simulation/sim_connection.h"
#include "logging.h"

namespace ssu {
namespace simulation {

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

    timer_.on_timeout.connect(boost::bind(&sim_packet::arrive, this));
    timer_.start(interval);
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
