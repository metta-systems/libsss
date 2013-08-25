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
#include <boost/signals2/signal.hpp>

namespace ssu {
namespace simulation {

class sim_timer_engine;

class simulator
{
    // All timers sorted by wake time.
    std::priority_queue<sim_timer_engine*> timers;
    boost::posix_time::ptime current_clock;

public:
    simulator();

    /**
     * Run simulation to the end.
     */
    void run();
    /**
     * Run just one simulation step.
     */
    void run_step();

    boost::posix_time::ptime current_time() const { return current_clock; }

    void enqueue_timer(sim_timer_engine* timer);
    void dequeue_timer(sim_timer_engine* timer);

    virtual void os_event_processing() {}

    typedef boost::signals2::signal<void (void)> step_event_signal;
    step_event_signal on_step_event;
};

} // simulation namespace
} // ssu namespace
