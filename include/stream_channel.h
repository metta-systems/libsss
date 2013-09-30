//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <queue>
#include <unordered_map>
#include "channel.h"
#include "base_stream.h"

namespace ssu {

// namespace private_ {
class stream_peer;
// } // private_ namespace

/**
 * Channel implementation for SSU streams.
 */
class stream_channel : public channel, public stream_protocol
{
    friend class base_stream; /// @fixme
    friend class stream_rx_attachment; /// @fixme
    friend class stream_tx_attachment; /// @fixme

    typedef channel super;

    /// Top-level stream used for connecting to services.
    std::shared_ptr<base_stream> root_{nullptr};

    /// Stream peer this channel is associated with.
    /// A stream_channel is always either a direct child of its stream_peer,
    /// or a child of a key_initiator which is a child of its stream_peer, @fixme
    /// so there should be no chance of this pointer ever dangling.
    stream_peer* peer_{nullptr};

    std::unordered_map<stream_id_t, stream_tx_attachment*> transmit_sids_; // Our SID namespace
    std::unordered_map<stream_id_t, stream_rx_attachment*> receive_sids_; // Peer's SID namespace
    counter_t transmit_sid_counter_{0}; // Next stream counter to assign.
    counter_t transmit_sid_acked_{0};   // Last acknowledged stream counter.
    counter_t received_sid_counter_{0};   // Last stream counter received.

    /// Closed stream IDs waiting for close acknowledgment.
    std::unordered_set<stream_id_t> closed_streams_;

    /**
     * Streams queued for transmission on this channel.
     * This should be a priority queue for simplicity of enqueueing.
     * We use deque with the limitation that only push_back() and pop_front() are used.
     */
    std::deque<base_stream*> sending_streams_;

    std::unordered_map<packet_seq_t, base_stream::packet> waiting_ack_;
    std::unordered_map<packet_seq_t, base_stream::packet> waiting_expiry_;

    // RxSID of stream on which we last received a packet -
    // this determines for which stream we send receive window info
    // when transmitting "bare" Ack packets.
    stream_id_t ack_sid_;

    // Handlers.
    void got_link_status_changed(link::status new_status);

public:
    stream_channel(std::shared_ptr<host> host, stream_peer* peer, const peer_id& id);
    ~stream_channel();

    inline stream_peer* target_peer() { return peer_; }

    inline std::shared_ptr<base_stream> root_stream() { return root_; }

    counter_t allocate_transmit_sid();

    void enqueue_stream(base_stream* stream);
    void dequeue_stream(base_stream* stream);

    /**
     * Detach all streams currently transmit-attached to this channel,
     * and send any of their outstanding packets back for retransmission.
     */
    void detach_all();

    void start(bool initiate) override;
    void stop() override;

    bool transmit_ack(byte_array &pkt, packet_seq_t ackseq, int ack_count) override;

    void acknowledged(packet_seq_t txseq, int npackets, packet_seq_t rxackseq) override;
    void missed(packet_seq_t txseq, int npackets) override;
    void expire(packet_seq_t txseq, int npackets) override;

    bool channel_receive(packet_seq_t pktseq, byte_array const& pkt) override;

    // Handlers.
    void got_ready_transmit();
};

} // namespace ssu
