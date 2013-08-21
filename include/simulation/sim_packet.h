//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include "link.h"
#include "timer.h"

namespace ssu {
namespace simulation {

class simulator;
class sim_host;
class sim_connection;

class sim_packet
{
    std::shared_ptr<simulator> simulator_;
    endpoint from_, to_;
    std::shared_ptr<sim_host> target_host_;
    byte_array data_;
    async::timer timer_;
    bool is_client_{false};

    void arrive();

public:
    sim_packet(std::shared_ptr<sim_host> source_host, endpoint const& src,
        std::shared_ptr<sim_connection> pipe, endpoint const& dst,
        byte_array data);
    ~sim_packet();
};

} // simulation namespace
} // ssu namespace
