//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "simulation/simulator.h"
#include "simulation/sim_timer_engine.h"

namespace ssu {
namespace simulation {

void simulator::run()
{
    while (!timers.empty()) {
        run_step();
    }
}

void simulator::run_step()
{
    sim_timer_engine* next = timers.top();
    timers.pop();

    assert(next->wake_time() >= current_time());

    // Move the virtual system clock forward to this event
    current_clock = next->wake_time();
    next->clear_wake_time();

    // Dispatch the event
    next->timeout();

    // Run any OS-specific pending event handling
    os_event_processing();

    // Notify interested listeners
    on_step_event();
}

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
