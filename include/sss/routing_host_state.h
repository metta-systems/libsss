//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <boost/signals2/signal.hpp>
#include <memory>
#include "routing/coordinator.h"

namespace sss {

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

} // sss namespace
