//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "ssu/simulation/sim_link.h"
#include "ssu/simulation/sim_host.h"
#include "ssu/simulation/sim_packet.h"

using namespace std;

namespace ssu {
namespace simulation {

sim_link::sim_link(shared_ptr<sim_host> host)
    : uia::comm::socket(host)
    , simulator_(host->get_simulator())
    , host_(host)
{}

sim_link::~sim_link()
{
    unbind();
}

bool
sim_link::bind(uia::comm::endpoint const& ep)
{
    assert(port_ == 0);

    if (ep.port() == 0) {
        int port = 1;
        for (; port < 65536 and host_->link_for(port) != nullptr;)
            ++port;

        assert(port < 65536);

        port_ = port;
    } else {
        port_ = ep.port();
    }

    host_->register_link_at(port_, shared_from_this());

    logger::debug() << "Bound virtual socket on " << ep;

    set_active(true);
    return true;
}

void
sim_link::unbind()
{
    if (port_ > 0)
    {
        host_->unregister_link_at(port_, shared_from_this());
        port_ = 0;
    }
    set_active(false);
}

// Target address must be routable to in order to send.
// Find the destination host in the "routing table" (a simple list of neighbors).
bool sim_link::send(uia::comm::endpoint const& ep, const char *data, size_t size)
{
    assert(port_ > 0);

    uia::comm::endpoint src;
    src.port(port_);
    shared_ptr<sim_host> dest_host = host_->neighbor_at(ep, src);
    if (!dest_host) {
        logger::warning() << "Unknown or non-adjacent target host " << ep;
        return false;
    }

    shared_ptr<sim_connection> pipe(host_->connection_at(src));
    assert(pipe);

    make_shared<sim_packet>(host_, src, pipe, ep, byte_array(data, size))->send();

    return true;
}

vector<uia::comm::endpoint>
sim_link::local_endpoints()
{
    vector<uia::comm::endpoint> result;
    for (auto ep : host_->local_endpoints()) {
        result.emplace_back(ep.address(), local_port());
    }
    return result;
}

} // simulation namespace
} // ssu namespace
