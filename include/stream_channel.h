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
    queue<base_stream*> tx_streams;

public:
    stream_channel(Host *h, StreamPeer *peer, const PeerId &peerid);
    ~stream_channel();

    inline stream_peer* target_peer() { return peer_; }

    void enqueue_stream(base_stream* stream);
    void dequeue_stream(base_stream* stream);

    /**
     * Detach all streams currently transmit-attached to this channel,
     * and send any of their outstanding packets back for retransmission.
     */
    void detach_all();


    virtual void start(bool initiate) override;
    virtual void stop() override;
};

} // namespace ssu
