//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <map>
#include <unordered_set>
#include <boost/signals2/signal.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include "protocol.h"
#include "host.h"
#include "peer_id.h"

namespace uia {
namespace routing {
class client;
} // routing namespace
} // uia namespace

namespace ssu {

class base_stream;
class stream_channel;

// namespace internal {

/**
 * Private helper class to keep information about a peer we are trying to establish connection with.
 * Contains cryptographic identifier as well as a set of possible endpoint addresses.
 * Keeps track of established sessions.
 * Communicates with resolver service to find reachable peer endpoints.
 */
class stream_peer : public stream_protocol
{
    friend class base_stream; // @fixme Use accessors n stuff.
    friend class stream_host_state; // @fixme used only to construct.
    friend class stream_channel; // @fixme Used to call channel_started() only.

    /**
     * Retry connection attempts for persistent streams once every minute.
     */
    static const async::timer::duration_type connect_retry_period;

    /**
     * Number of stall warnings we get from our primary stream
     * before we start a new lookup/key exchange phase to try replacing it.
     */
    static constexpr int stall_warnings_max = 3;

    std::shared_ptr<host> host_;               ///< Per-host state.
    const peer_id         remote_id_;          ///< Host ID of target.
    stream_channel*       primary_channel_{0}; ///< Current primary channel.
    int                   stall_warnings_{0};  ///< Stall warnings before new lookup.

    // Routing state:
    std::unordered_set<uia::routing::client*> lookups_; ///< Outstanding lookups in progress
    async::timer reconnect_timer_;                      ///< For persistent lookup requests
    // std::unordered_set<uia::routing::client*> connected_routing_clients_; // Set of RegClients we've connected to so far

    // For channels under construction:
    std::unordered_set<endpoint> locations_; ///< Potential locations known
    std::map<link_endpoint, std::shared_ptr<negotiation::key_initiator>> key_exchanges_initiated_;

    // All existing streams involving this peer.
    std::unordered_set<base_stream*> all_streams_;
    // All streams that have USIDs, registered by their USIDs
    // @todo change into weak_ptrs<base_stream>
    std::unordered_map<unique_stream_id_t, base_stream*> usid_streams_;

private:
    inline peer_id remote_host_id() const { return remote_id_; }

    /**
     * Connect to routing change signals to find peer endpoints.
     */
    // void observe_routing(ssu::routing::client* client);

    /**
     * Initiate a key exchange attempt to a given endpoint,
     * if such an attempt isn't already in progress.
     */
    void initiate_key_exchange(link* l, const endpoint& ep);

    /**
     * Called by stream_channel::start() whenever a new channel
     * (either incoming or outgoing) successfully starts.
     */
    void channel_started(stream_channel* channel);

    /**
     * Clear the peer's current primary channel.
     */
    void clear_primary_channel();

    // Handlers.
    void completed(std::shared_ptr<negotiation::key_initiator> ki, bool success);
    void primary_status_changed(link::status new_status);

    // Routing client handlers @todo
    // void lookupDone(const SST::PeerId &id, const Endpoint &loc, const RegInfo &info);
    // void regClientDestroyed(QObject *obj);
    void retry_timeout();

    struct private_tag {};

public:
    stream_peer(std::shared_ptr<host> const& host, peer_id const& remote_id, private_tag);
    ~stream_peer();

    /**
     * Initiate a connection attempt to target host by any means possible,
     * hopefully at some point resulting in an active primary channel.
     * Eventually emits a on_channel_connected or on_channel_failed signal.
     */
    void connect_channel();

    /**
     * Supply an endpoint hint that may be useful for finding this peer.
     */
    void add_location_hint(endpoint const& hint);

    typedef boost::signals2::signal<void (void)> channel_state_signal;
    typedef boost::signals2::signal<void (link::status)> link_status_changed_signal;

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

// } // internal namespace
} // ssu namespace
