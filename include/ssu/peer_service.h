#pragma once

/**
 * Base class for implementing peer-to-peer services.
 * Integrates an ssu::server to handle incoming connections
 * and keeps track of outgoing connections you're trying to set up.
 */
class peer_service
{
    /**
     * Minimum delay between successive connection attempts - 1 minute
     */
    static reconnect_delay = minutes(1);

    ssu::server server_;                           // To accept incoming streams
    const std::string service_name_;               // Name of service we provide
    const std::string protocol_name_;              // Name of protocol we support

    std::map<peer_id, stream*> out;                // Outgoing streams by host ID
    std::map<peer_id, std::hash_map<stream*>> in;  // Incoming streams by host ID

public:
    peer_service(string service_name, string service_desc,
                 string protocol_name, string protocol_desc);

    /**
     * Create a primary outgoing connection if one doesn't already exist.
     * @param  hostId target host EID to connect to.
     * @return a new Stream connection to the given peer.
     */
    stream *connect_to_peer(peer_id eid);

    /**
     * Create a new outgoing connection to a given peer, destroying the old primary connection if any.
     * @param  hostId [description]
     * @return        [description]
     */
    stream *reconnect_to_peer(peer_id eid);

    /**
     * Destroy any outgoing connection we may have to a given peer.
     * @param hostId [description]
     */
    void disconnect_from_peer(peer_id eid);

    /**
     * Destroy all connections, outgoing AND incoming, with a given peer.
     * @param hostId [description]
     */
    void disconnect_peer(peer_id eid);

    /**
     * Return the current outgoing stream to a given peer, nullptr if none.
     * @param  hostId [description]
     * @return        [description]
     */
    inline stream *out_stream(peer_id eid) {
        return out.value(eid);
    }

    /**
     * Returns true if an outgoing stream exists and is connected.
     * @param  id [description]
     * @return    [description]
     */
    inline bool is_out_connected(peer_id eid)
    {
        stream *s = out.value(eid);
        return s && s->is_connected();
    }

    /**
     * Return a set of incoming streams from given peer.
     */
    inline std::hash_map<stream*> in_streams(peer_id eid) {
        return in.value(eid);
    }


    boost::signals2::signal<void(stream*)> on_out_stream_connected;
    boost::signals2::signal<void(stream*)> on_out_stream_disconnected;
    boost::signals2::signal<void(stream*)> on_in_stream_connected;
    boost::signals2::signal<void(stream*)> on_in_stream_disconnected;
    boost::signals2::signal<void(peer_id const&)> on_peer_status_changed;

};
