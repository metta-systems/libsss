#pragma once

#include "host.h"

namespace ssu {
namespace simulation {

class sim_host : public host
{
    std::shared_ptr<simulator> simulator_;

public:
    sim_host(std::shared_ptr<simulator> sim);

    boost::posix_time::ptime current_time() override;
    std::unique_ptr<async::timer_engine> create_timer_engine_for(async::timer* t) override;
};

} // simulation namespace
} // ssu namespace
