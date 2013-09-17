//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

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
    std::shared_ptr<base_stream> root_{nullptr};
    typedef channel super;
    stream_peer* peer_{nullptr};

    std::unordered_map<stream_id_t, stream_tx_attachment*> transmit_sids_; // Our SID namespace
    std::unordered_map<stream_id_t, stream_rx_attachment*> receive_sids_; // Peer's SID namespace
    counter_t transmit_sid_counter_{0}; // Next stream counter to assign.
    counter_t transmit_sid_acked_{0};   // Last acknowledged stream counter.
    counter_t received_sid_counter_{0};   // Last stream counter received.

    /**
     * Streams queued for transmission on this channel.
     * This should be a priority queue for simplicity of enqueueing.
     */
    // priority_queue<base_stream*> tx_streams;

    typedef uint64_t tx_seq_id;///@fixme

    std::unordered_map<tx_seq_id, base_stream::packet*> waiting_ack_;
    std::unordered_map<tx_seq_id, base_stream::packet*> waiting_expiry_;

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

    bool transmit_ack(byte_array &pkt, uint64_t ackseq, unsigned ackct) override;

    void acknowledged(uint64_t txseq, int npackets, uint64_t rxackseq) override;
    void missed(uint64_t txseq, int npackets) override;
    void expire(uint64_t txseq, int npackets) override;

    bool channel_receive(uint64_t pktseq, byte_array &pkt) override;
};

} // namespace ssu
