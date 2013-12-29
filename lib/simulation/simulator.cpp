//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include <boost/date_time/posix_time/posix_time.hpp>
#include "simulation/simulator.h"
#include "simulation/sim_timer_engine.h"
#include "logging.h"

namespace ssu {
namespace simulation {

simulator::simulator()
    : current_clock_(boost::posix_time::from_iso_string("20000101T000000"))
{
}

simulator::~simulator()
{
    posted_actions_.clear();
    timers_.clear(); // @todo Either keep shared pointers in timers_ or call delete on them here.
}

void simulator::run()
{
    while (!timers_.empty()) {
        run_step();
    }
    logger::info() << "Simulation completed. #####";
}

void simulator::run_actions()
{
    while (posted_actions_.size() > 0)
    {
        posted_actions_.front()();
        posted_actions_.pop_front();
    }
}

void simulator::run_step()
{
    sim_timer_engine* next = timers_.front();
    timers_.erase(timers_.begin());

    assert(next->wake_time() >= current_time());

    // Move the virtual system clock forward to this event
    current_clock_ = next->wake_time();
    next->clear_wake_time();

    logger::info() << "Simulation step: time now " << current_clock_ << " #####";

    // Run posted mainloop actions
    run_actions();

    // Dispatch the event
    next->timeout();

    // Run any OS-specific pending event handling
    os_event_processing();

    // Notify interested listeners
    on_step_event();
}

void simulator::enqueue_timer(sim_timer_engine* timer)
{
    size_t i = 0;
    for (; i < timers_.size(); ++i)
    {
        if (timer->wake_time() < timers_[i]->wake_time()) {
            break;
        }
    }
    timers_.insert(timers_.begin() + i, timer);
}

void simulator::dequeue_timer(sim_timer_engine* timer)
{
    for (auto it = find(timers_.begin(), timers_.end(), timer); it != timers_.end();)
    {
        timers_.erase(it);
        it = find(timers_.begin(), timers_.end(), timer);
    }
}

} // simulation namespace
} // ssu namespace
