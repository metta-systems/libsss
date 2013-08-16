//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include "byte_array.h"
#include "link.h"
#include "link_channel.h"

namespace ssu {

class host;
class channel_armor;

/**
 * Abstract base class representing a channel between a local Socket and a remote endpoint.
 */
class channel : public link_channel
{
    std::weak_ptr<host> host_;
    channel_armor* armor_;

public:
    static const int hdrlen = 8/*XXX*/;

	virtual void start(bool initiate);
	virtual void stop();

    /**
     * Set the encryption/authentication method for this channel.
     * This MUST be set before a new channel can be activated.
     */
    inline void set_armor(channel_armor *armor) {
        armor_ = armor;
    }
    inline channel_armor* armor() {
        return armor_;
    }

protected:
    /**
     * Main method for upper-layer subclass to receive a packet on a flow.
     * Should return true if the packet was processed and should be acked,
     * or false to silently pretend we never received the packet.
     */
    virtual bool channel_receive(uint64_t pktseq, byte_array &pkt) = 0;

    /**
     * Create and transmit a packet for acknowledgment purposes only.
     * Upper layer may override this if ack packets should contain
     * more than an just an empty channel payload.
     */
    virtual bool transmit_ack(byte_array &pkt, uint64_t ackseq, unsigned ackct);

    virtual void acknowledged(uint64_t txseq, int npackets, uint64_t rxackseq);
    virtual void missed(uint64_t txseq, int npackets);
    virtual void expire(uint64_t txseq, int npackets);

private:
    /**
     * Called by link to dispatch a received packet to this channel.
     * @param msg [description]
     * @param src [description]
     */
    void receive(const byte_array& msg, const link_endpoint& src) override;
};

} // namespace ssu
