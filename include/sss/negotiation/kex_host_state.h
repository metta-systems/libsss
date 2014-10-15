#pragma once

#include <memory>
#include <unordered_map>
#include "arsenal/byte_array.h"
#include "comm/socket_endpoint.h"
#include "sss/negotiation/kex_initiator.h"

namespace sss {

/**
 * Mixin for host state that manages the key exchange state.
 */
class kex_host_state
{
    /**
     * Initiators by nonce.
     */
    std::unordered_map<byte_array, negotiation::kex_initator_ptr_t> dh_initiators_;
    /**
     * Initiators by endpoint.
     * Used for handling R0 packets during hole-punching.
     */
    std::unordered_multimap<uia::comm::endpoint, negotiation::kex_initator_ptr_t> ep_initiators_;

public:
    typedef std::unordered_multimap<uia::comm::endpoint, negotiation::kex_initator_ptr_t>::iterator
        ep_iterator;

    negotiation::kex_initator_ptr_t get_initiator(byte_array nonce);
    std::pair<ep_iterator, ep_iterator> get_initiators(uia::comm::endpoint const& ep);

    void register_dh_initiator(byte_array const& nonce, uia::comm::endpoint const& ep,
        negotiation::kex_initator_ptr_t ki);
    void unregister_dh_initiator(byte_array const& nonce, uia::comm::endpoint const& ep);
};

} // sss namespace
