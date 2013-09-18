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
#include "channel_armor.h"

namespace ssu {

class host;

/**
 * Abstract base class representing a channel between a local Socket and a remote endpoint.
 */
class channel : public link_channel
{
    typedef link_channel super;

    std::shared_ptr<host>          host_;
    std::unique_ptr<channel_armor> armor_; // armors cannot be shared.

    link::status                   link_status_;

    byte_array                     tx_channel_id_;
    byte_array                     rx_channel_id_;

public:
    static constexpr int header_len = 8/*XXX*/;

    channel(std::shared_ptr<host> host);
    virtual ~channel();

	void start(bool initiate) override;
	void stop() override;

    virtual int may_transmit();

    inline byte_array tx_channel_id() { return tx_channel_id_; }
    inline byte_array rx_channel_id() { return rx_channel_id_; }

    inline void set_channel_ids(byte_array const& tx_id, byte_array const& rx_id) {
        tx_channel_id_ = tx_id;
        rx_channel_id_ = rx_id;
    }

    /**
     * Set the encryption/authentication method for this channel.
     * This MUST be set before a new channel can be activated.
     */
    inline void set_armor(std::unique_ptr<channel_armor> armor) {
        armor_ = std::move(armor);
    }

    /**
     * Return the current link status as observed by this channel.
     */
    inline link::status link_status() const { return link_status_; }

    inline void set_link_status(link::status new_status) { link_status_ = new_status; }

    typedef boost::signals2::signal<void (link::status)> link_status_changed_signal;

    /// Indicates when this channel observes a change in link status.
    link_status_changed_signal on_link_status_changed;

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
