//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <boost/signals2/signal.hpp>
#include <memory>
#include "coordinator.h"

namespace ssu {

/**
 * We store a routing::client_coordinator which keeps track of our
 * peer search requests. If you need different routing behavior, either extend
 * client_coordinator or replace routing_host_state with your own.
 */
class routing_host_state
{
public:
    /**
     * We can't create coordinator in ctor, because it needs shared_ptr to host,
     * instead, it is intialized upon requesting first connection.
     * As long as your connections include an endpoint address, coordinator
     * need not be instantiated, saving memory and routing layer overhead.
     */
    std::shared_ptr<uia::routing::client_coordinator> coordinator;
};

} // ssu namespace
