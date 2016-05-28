//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include "uia/host.h"
#include "sss/internal/stream_host_state.h"
#include "sss/internal/routing_host_state.h"
#include "sss/forward_ptrs.h"

class settings_provider;

namespace sss {

/**
 * This class encapsulates all per-host state used by the sss protocol.
 * By centralizing this state here instead of using global/static variables,
 * the host environment can be virtualized for simulation purposes
 * and multiple sss instances can be run in one process.
 *
 * It is the client's responsibility to ensure that a host object
 * is not destroyed while any sss objects still refer to it.
 *
 * Example: it is customary to create a shared_ptr to host.
 * @snippet doc/snippets.cpp Creating a host
 */
class host : public uia::host,
             public stream_host_state,
             public routing_host_state
{
};

} // sss namespace
