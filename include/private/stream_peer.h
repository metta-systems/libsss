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
#include "protocol.h"

namespace ssu {
namespace private {

/**
 * Private helper class to keep information about a peer we are trying to establish connection with.
 * Contains cryptographic identifier as well as a set of possible endpoint addresses.
 * Keeps track of established sessions.
 * Communicates with resolver service to find reachable peer endpoints.
 */
class stream_peer : public stream_protocol
{
    // Retry connection attempts for persistent streams once every minute.
    static const posix_time::ptime connect_retry_period = minutes(1);

    // Number of stall warnings we get from our primary stream
    // before we start a new lookup/key exchange phase to try replacing it.
    static const int stall_warnings_max = 3;

    std::weak_ptr<host> host_;         ///< Per-host state.
    const peer_id       remote_id_;    ///< Host ID of target.
    stream_channel*     primary_channel_{0}; ///< Current primary channel.
    int stall_warnings_{0};
    // Routing state:
    //lookups_;
    //reconnect_timer_;
    //connected_routing_clients_;
    // Channels under construction:
    //set<endpoint> locations_; ///< Potential locations known
    //map<link_endpoint, key_initiator*> key_exchanges_initiated_;

    // All existing streams involving this peer.
    set<base_stream*> all_streams_;
    // All streams that have USIDs, registered by their USIDs
    map<unique_stream_id_t, base_stream*> usid_streams_;


    stream_peer(std::shared_ptr<host>& host, const peer_id& remote_id);
    ~stream_peer();

    inline peer_id remote_host_id() const { return remote_id_; }

    // connect to routing change signals to find peer endpoints:
    // void observe_routing(ssu::routing::client* client);

    /**
     * Initiate a connection attempt to target host by any means possible,
     * hopefully at some point resulting in an active primary channel.
     * Eventually emits a on_channel_connected or on_channel_failed signal.
     */
    void connect_channel();

    // Initiate a key exchange attempt to a given endpoint,
    // if such an attempt isn't already in progress.
    void initiate_key_exchange(link* l, const endpoint& ep);

    // Called by StreamFlow::start() whenever a new flow
    // (either incoming or outgoing) successfully starts.
    void channel_started(stream_channel* channel);

    // Clear the peer's current primary flow.
    void clear_primary_channel();

public:
    /// Supply an endpoint hint that may be useful for finding this peer.
    void add_location_hint(const endpoint& hint);

    typedef boost::signals2::signal<void (void)> channel_state_signal;
    channel_state_signal on_channel_connected; ///< Primary flow connection attempt succeeded.
    channel_state_signal on_channel_failed; ///< Connection attempt or the primary flow failed.


};

} // private namespace
} // ssu namespace
