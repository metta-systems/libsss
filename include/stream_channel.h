//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include "channel.h"

namespace ssu {

/**
 * Channel implementation for SSU streams.
 */
class stream_channel : public channel
{
    stream_peer* peer_;

    /**
     * Streams queued for transmission on this channel.
     * This should be a priority queue for simplicity of enqueueing.
     */
    priority_queue<base_stream*> tx_streams;

    std::unordered_map<tx_seq_id, packet*> waiting_ack_;
    std::unordered_map<tx_seq_id, packet*> waiting_expiry_;

public:
    stream_channel(std::shared_ptr<host> host, stream_peer* peer, const peer_id& id);
    ~stream_channel();

    inline stream_peer* target_peer() { return peer_; }

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

    void acknowledged(quint64 txseq, int npackets, quint64 rxackseq) override;
    void missed(quint64 txseq, int npackets) override;
    void expire(quint64 txseq, int npackets) override;
};

} // namespace ssu
