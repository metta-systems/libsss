//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <queue>
#include <boost/date_time/posix_time/posix_time_types.hpp>

namespace ssu {
namespace simulation {

class sim_timer_engine;

class simulator
{
    // All timers sorted by wake time.
    std::priority_queue<sim_timer_engine*> timers;

public:
    boost::posix_time::ptime current_time();

    void enqueue_timer(sim_timer_engine* timer);
    void dequeue_timer(sim_timer_engine* timer);
};

} // simulation namespace
} // ssu namespace