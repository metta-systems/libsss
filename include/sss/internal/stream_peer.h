//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2015, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <map>
#include <unordered_set>
#include <boost/signals2/signal.hpp>
#include <boost/signals2/connection.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include "sss/framing/stream_protocol.h"
#include "sss/host.h"
#include "sss/streams/base_stream.h"
#include "sss/internal/stream_host_state.h"
#include "sss/forward_ptrs.h"
#include "uia/peer.h"

namespace uia {
namespace routing {

class client;
class client_profile;

struct routing_coordination
{
    // Routing state:
    std::unordered_set<uia::routing::client*> lookups_; ///< Outstanding lookups in progress
    sss::async::timer reconnect_timer_;                 ///< For persistent lookup requests
    // Set of RegClients we've connected to so far: @todo move to routing
    std::unordered_set<uia::routing::client*> connected_routing_clients_;

    routing_coordination(std::shared_ptr<sss::host> host);
};

} // routing namespace
} // uia namespace

namespace sss {

class base_stream;
class stream_channel;

namespace internal {

/**
 * Private helper class to keep information about a peer we are establishing connection with.
 * Contains cryptographic identifier as well as a set of possible endpoint addresses.
 * Keeps track of established sessions.
 * Communicates with resolver service to find reachable peer endpoints.
 *
 * uia::peer
 * stream_peer
 *  - list of active streams
 *      - both connected to channels
 *      - and not bound anywhere (waiting for channel or idle)
 * routing_peer
 *  - routing state
 *      - peer IP lookups
 *      - DHT state related to this peer's online status
 *
 * For the purposes of channel management, it might be necessary to split this class into
 * uia::channel_peer that would handle channels and DHT state and sss::stream_peer that would
 * add stream management.
 *
 * Perhaps, routing coordination must also be extracter into routing::routing_peer class.
 *
 */
class stream_peer : public uia::internal::peer, public stream_protocol
{
    friend class sss::stream_host_state; // @fixme used only to construct (stream_creator)

    // All existing streams involving this peer.
    std::unordered_set<base_stream_ptr> all_streams_;
    // All streams that have USIDs, registered by their USIDs
    std::unordered_map<unique_stream_id_t, base_stream_wptr> usid_streams_;

    uia::routing::routing_coordination coord_;

    inline bool no_lookups_possible()
    {
        return coord_.lookups_.empty() and key_exchanges_initiated_.empty();
    }

    /**
     * Connect to routing change signals to find peer endpoints.
     */
    // void observe_routing(uia::routing::client* client);

    // Routing client handlers
    void routing_client_ready(uia::routing::client* rc);
    void connect_routing_client(uia::routing::client* rc);

    // Routing client handlers
    void lookup_done(uia::routing::client* rc,
                     uia::peer_identity const& target_peer,
                     uia::comm::endpoint const& peer_endpoint,
                     uia::routing::client_profile const& peer_profile);
    void regclient_destroyed(uia::routing::client* rc);
    void retry_timeout();

    struct private_tag
    {
    };

public:
    stream_peer(host_ptr const& host, uia::peer_identity const& remote_id, private_tag);
    ~stream_peer();
};

} // internal namespace
} // sss namespace
