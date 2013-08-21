#pragma once

#include "timer_engine.h"

namespace ssu {
namespace simulation {

class sim_timer_engine : public timer_engine
{
public:
    sim_timer_engine(timer* t, boost::asio::io_service& io_service);
    ~sim_timer_engine();

    void start(duration_type interval) override;
    void stop() override;
};

} // simulation namespace
} // ssu namespace
