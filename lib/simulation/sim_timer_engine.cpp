//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "ssu/simulation/sim_timer_engine.h"
#include "ssu/simulation/simulator.h"

namespace ssu {
namespace simulation {

sim_timer_engine::sim_timer_engine(async::timer* t, std::shared_ptr<simulator> sim)
    : timer_engine(t)
    , simulator_(sim)
    , wake_(boost::date_time::not_a_date_time)
{
}

sim_timer_engine::~sim_timer_engine()
{
    stop();
}

void sim_timer_engine::start(duration_type interval)
{
    stop();

    wake_ = simulator_->current_time() + interval;
    simulator_->enqueue_timer(this);
}

void sim_timer_engine::stop()
{
    if (wake_.is_not_a_date_time()) {
        return;
    }
    simulator_->dequeue_timer(this);
    clear_wake_time();
}

} // simulation namespace
} // ssu namespace
