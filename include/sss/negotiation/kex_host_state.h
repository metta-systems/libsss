#pragma once

#include <memory>
#include <unordered_map>
#include "arsenal/byte_array.h"
#include "comm/socket_endpoint.h"
#include "sss/negotiation/key_initiator.h"

namespace sss {

/**
 * Mixin for host state that manages the key exchange state.
 */
class key_host_state
{
    /**
     * Initiators by nonce.
     */
    std::unordered_map<byte_array, std::shared_ptr<negotiation::key_initiator>> dh_initiators_;
    /**
     * Initiators by endpoint.
     * Used for handling R0 packets during hole-punching.
     */
    std::unordered_multimap<uia::comm::endpoint,
        std::shared_ptr<negotiation::key_initiator>> ep_initiators_;

public:
    typedef std::unordered_multimap<uia::comm::endpoint,
        std::shared_ptr<negotiation::key_initiator>>::iterator
        ep_iterator;

    std::shared_ptr<negotiation::key_initiator> get_initiator(byte_array nonce);
    std::pair<ep_iterator, ep_iterator> get_initiators(uia::comm::endpoint const& ep);

    void register_dh_initiator(byte_array const& nonce, uia::comm::endpoint const& ep,
        std::shared_ptr<sss::negotiation::key_initiator> ki);
    void unregister_dh_initiator(byte_array const& nonce, uia::comm::endpoint const& ep);
};

} // sss namespace
