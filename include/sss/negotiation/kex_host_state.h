//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2015, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <memory>
#include <unordered_map>
#include "arsenal/byte_array.h"
#include "uia/comm/socket_endpoint.h"
#include "sss/negotiation/kex_initiator.h"
#include "sss/forward_ptrs.h"

namespace sss {

/**
 * Mixin for the host state that manages the key exchange state.
 */
class kex_host_state
{
    /**
     * Initiators by endpoint.
     * Used for handling R0 packets during hole-punching.
     */
    std::unordered_map<uia::comm::endpoint, negotiation::kex_initiator_ptr> initiators_;

public:
    negotiation::kex_initiator_ptr get_initiator(uia::comm::endpoint const& ep);
    void register_initiator(uia::comm::endpoint const& ep,
                            negotiation::kex_initiator_ptr ki);
    void unregister_initiator(uia::comm::endpoint const& ep);
};

} // sss namespace
