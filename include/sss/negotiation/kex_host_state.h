//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <memory>
#include <unordered_map>
#include "arsenal/byte_array.h"
#include "comm/socket_endpoint.h"
#include "sss/negotiation/kex_initiator.h"
#include "sss/forward_ptrs.h"

namespace sss {

/**
 * Mixin for host state that manages the key exchange state.
 */
class kex_host_state
{
    /**
     * Initiators by nonce.
     */
    std::unordered_map<byte_array, negotiation::kex_initiator_ptr> initiators_;
    /**
     * Initiators by endpoint.
     * Used for handling R0 packets during hole-punching.
     */
    std::unordered_multimap<uia::comm::endpoint, negotiation::kex_initiator_ptr> ep_initiators_;

public:
    using ep_iterator =
        std::unordered_multimap<uia::comm::endpoint, negotiation::kex_initiator_ptr>::iterator;

    negotiation::kex_initiator_ptr get_initiator(byte_array nonce);
    std::pair<ep_iterator, ep_iterator> get_initiators(uia::comm::endpoint const& ep);

    void register_initiator(byte_array const& nonce,
                            uia::comm::endpoint const& ep,
                            negotiation::kex_initiator_ptr ki);
    void unregister_initiator(byte_array const& nonce, uia::comm::endpoint const& ep);
};

} // sss namespace
