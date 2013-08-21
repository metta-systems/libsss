//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include "timer_engine.h"

namespace ssu {
namespace simulation {

class simulator;

class sim_timer_engine : public async::timer_engine
{
    std::shared_ptr<simulator> simulator_;
    boost::posix_time::ptime wake_;

public:
    sim_timer_engine(async::timer* t, std::shared_ptr<simulator> sim);
    ~sim_timer_engine();

    void start(duration_type interval) override;
    void stop() override;
};

} // simulation namespace
} // ssu namespace
