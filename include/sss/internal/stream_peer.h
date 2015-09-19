//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@atta-metta.net>
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
#include "sss/channels/peer_identity.h"
#include "sss/channels/stream_channel.h"
#include "sss/forward_ptrs.h"

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
};

} // routing namespace
} // uia namespace

namespace sss {

class base_stream;
class stream_channel;

using channel_key = std::array<uint8_t, 32>;

namespace internal {

/**
 * Private helper class to keep information about a peer we are trying to establish connection with.
 * Contains cryptographic identifier as well as a set of possible endpoint addresses.
 * Keeps track of established sessions.
 * Communicates with resolver service to find reachable peer endpoints.
 */
class stream_peer : public stream_protocol
{
    friend class sss::base_stream;       // @fixme Use accessors n stuff.
    friend class sss::stream_host_state; // @fixme used only to construct.
    friend class sss::stream_channel;    // @fixme Used to call channel_started() only.

    host_ptr host_;                                      ///< Per-host state.
    const uia::peer_identity remote_id_;                 ///< Host ID of target.
    std::map<channel_key, stream_channel_ptr> channels_; ///< Currently established channels.

    /// @internal
    boost::signals2::connection primary_channel_link_status_connection_;

    // For channels under construction:
    std::unordered_set<uia::comm::endpoint> locations_; ///< Potential peer locations known

    // @fixme key on sockets or permanent IDs? hmm
    std::map<uia::comm::socket_endpoint, negotiation::kex_initiator_ptr> key_exchanges_initiated_;

    // All existing streams involving this peer.
    std::unordered_set<base_stream_ptr> all_streams_;
    // All streams that have USIDs, registered by their USIDs
    // @todo change into weak_ptrs<base_stream>
    std::unordered_map<unique_stream_id_t, base_stream_wptr> usid_streams_;

    uia::routing::routing_coordination coord_;

private:
    inline uia::peer_identity remote_host_id() const { return remote_id_; }

    inline bool no_lookups_possible()
    {
        return coord_.lookups_.empty() and key_exchanges_initiated_.empty();
    }

    /**
     * Connect to routing change signals to find peer endpoints.
     */
    // void observe_routing(uia::routing::client* client);

    /**
     * Initiate a key exchange attempt to a given endpoint,
     * if such an attempt isn't already in progress.
     */
    void initiate_key_exchange(uia::comm::socket_wptr s, uia::comm::endpoint const& ep);
    // ^^ @todo Do we care about EPs now at all? new key exchange would initiate a new channel
    // each time this is called.

    /**
     * Called by stream_channel::start() whenever a new channel
     * (either incoming or outgoing) successfully starts.
     */
    void channel_started(stream_channel* channel);

    /**
     * Clear the peer's current primary channel.
     */
    // void clear_primary_channel();

    // Handlers.
    void completed(std::shared_ptr<negotiation::kex_initiator> ki, bool success); // KEX inited
    void primary_status_changed(uia::comm::socket::status new_status);

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

    /**
     * Initiate a connection attempt to target host by any means possible,
     * hopefully at some point resulting in an active primary channel.
     * Eventually emits a on_channel_connected or on_channel_failed signal.
     */
    // void connect_channel();

    /**
     * Supply an endpoint hint that may be useful for finding this peer.
     */
    void add_location_hint(uia::comm::endpoint const& hint);

    using channel_state_signal       = boost::signals2::signal<void(void)>;
    using link_status_changed_signal = boost::signals2::signal<void(uia::comm::socket::status)>;

    /**
     * Primary channel connection attempt succeeded.
     */
    channel_state_signal on_channel_connected;
    /**
     * Connection attempt or the primary channel failed.
     */
    channel_state_signal on_channel_failed;
    /**
     * Indicates when this stream peer observes a change in link status.
     */
    link_status_changed_signal on_link_status_changed;
};

} // internal namespace
} // sss namespace
