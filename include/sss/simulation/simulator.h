//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <deque>
#include <vector>
#include <functional>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/signals2/signal.hpp>

namespace sss {
namespace simulation {

class sim_timer_engine;

class simulator
{
    /// All timers sorted by wake time.
    std::vector<sim_timer_engine*> timers_;
    /// Actions posted from the threads to be run by the main loop at start of the next step.
    std::deque<std::function<void(void)>> posted_actions_;
    boost::posix_time::ptime current_clock_;

    /**
     * Run posted actions.
     */
    void run_actions();

public:
    simulator();
    ~simulator();

    /**
     * Run simulation to the end.
     */
    void run();
    /**
     * Run just one simulation step.
     */
    void run_step();

    boost::posix_time::ptime current_time() const { return current_clock_; }

    void enqueue_timer(sim_timer_engine* timer);
    void dequeue_timer(sim_timer_engine* timer);

    void post(std::function<void(void)> f) { posted_actions_.emplace_back(f); }

    virtual void os_event_processing() {}

    using step_event_signal = boost::signals2::signal<void (void)>;
    step_event_signal on_step_event;
};

} // simulation namespace
} // sss namespace
