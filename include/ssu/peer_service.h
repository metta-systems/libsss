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
#include <unordered_map>
#include <string>
#include <boost/signals2/signal.hpp>
#include "ssu/server.h"
#include "ssu/stream.h"

namespace ssu {

/**
 * Base class for implementing peer-to-peer services.
 * Integrates an ssu::server to handle incoming connections
 * and keeps track of outgoing connections you set up.
 */
class peer_service
{
    /**
     * Minimum delay between successive connection attempts - 1 minute
     */
    static reconnect_delay = minutes(1);

    ssu::server server_;                                // To accept incoming streams
    const std::string service_name_;                    // Name of service we provide
    const std::string protocol_name_;                   // Name of protocol we support

    std::map<peer_id, stream*> out;                     // Outgoing streams by host ID
    std::map<peer_id, std::unordered_map<stream*>> in;  // Incoming streams by host ID

public:
    /**
     * Create a peer service for given service and protocol.
     */
    peer_service(std::string service_name, std::string service_desc,
                 std::string protocol_name, std::string protocol_desc);

    /**
     * Create a primary outgoing connection if one doesn't already exist.
     * @param  hostId target host EID to connect to.
     * @return        a new stream connection to the given peer.
     */
    stream *connect_to_peer(peer_id eid);

    /**
     * Create a new outgoing connection to a given peer, destroying
     * the old primary connection if any.
     * @param  hostId target host EID to reconnect to.
     * @return        a new stream connection to the given peer.
     */
    stream *reconnect_to_peer(peer_id eid);

    /**
     * Destroy any outgoing connection we may have to a given peer.
     * @param hostId target host EID to disconnect from.
     */
    void disconnect_from_peer(peer_id eid);

    /**
     * Destroy all connections, outgoing AND incoming, with a given peer.
     * @param hostId target host EID to disconnect from.
     */
    void disconnect_peer(peer_id eid);

    /**
     * Return the current outgoing stream to a given peer, nullptr if none.
     * @param  hostId target host EID.
     * @return        existing outgoing stream connection to the given peer or nullptr.
     */
    inline stream *out_stream(peer_id eid) {
        return out[eid];
    }

    /**
     * Returns true if an outgoing stream exists and is connected.
     * @param  id target host EID.
     * @return    true is outgoing stream connection exists, otherwise false.
     */
    inline bool is_out_connected(peer_id eid)
    {
        stream *s = out.value(eid);
        return s && s->is_connected();
    }

    /**
     * Return a set of incoming streams from given peer.
     */
    inline std::unordered_map<stream*> in_streams(peer_id eid) {
        return in[eid];
    }

    boost::signals2::signal<void(stream*)>         on_out_stream_connected;
    boost::signals2::signal<void(stream*)>         on_out_stream_disconnected;
    boost::signals2::signal<void(stream*)>         on_in_stream_connected;
    boost::signals2::signal<void(stream*)>         on_in_stream_disconnected;
    boost::signals2::signal<void(peer_id const&)>  on_peer_status_changed;
};

} // ssu namespace
