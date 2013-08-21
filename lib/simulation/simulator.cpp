//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "simulation/simulator.h"

namespace ssu {
namespace simulation {

void simulator::enqueue_timer(sim_timer_engine* timer)
{
    timers.push(timer);
}

void simulator::dequeue_timer(sim_timer_engine* timer)
{
    // timers.remove(timer);
    // removing from a priority_queue is non-trivial
}

} // simulation namespace
} // ssu namespace
