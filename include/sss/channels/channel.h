//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include "arsenal/byte_array.h"
#include "comm/socket.h"
#include "comm/socket_channel.h"
#include "sss/framing/stream_protocol.h"
#include "sss/channels/channel_armor.h"

namespace sss {

class host;

/**
 * Abstract base class representing a channel between a local link and a remote endpoint.
 */
class channel : public uia::comm::socket_channel
{
    friend class base_stream; // @fixme *sigh*

    using super = uia::comm::socket_channel;

    class private_data;
    std::unique_ptr<private_data> pimpl_;  ///< Most of the state is hidden from interface.

    std::unique_ptr<channel_armor> armor_;         ///< Armors cannot be shared.
    /// Per-direction unique channel IDs for this channel.
    /// Stream layer uses these in assigning USIDs to new streams.
    byte_array   tx_channel_id_;                   ///< Transmit ID of the channel.
    byte_array   rx_channel_id_;                   ///< Receive ID of the channel.
    uia::comm::socket::status link_status_{uia::comm::socket::status::down}; ///< Link online status.

    /**
     * When packet sequence reaches this number, the channel is no longer usable
     * and must be terminated.
     * It is advised to create a new channel long before reaching this limit.
     */
    static constexpr packet_seq_t max_packet_sequence = ~0ULL;

public:
    static constexpr size_t header_len = 0; // @fixme Get rid of this

    channel(std::shared_ptr<host> host);
    virtual ~channel();

    virtual std::shared_ptr<host> get_host();

    /// Start the channel.
    void start(bool initiate) override;
    /// Stop the channel.
    void stop() override;

    /// Check congestion control state and return the number of new packets,
    /// if any, that flow control says we may transmit now.
    size_t may_transmit() override;

    inline byte_array tx_channel_id() {
        return tx_channel_id_;
    }
    inline byte_array rx_channel_id() {
        return rx_channel_id_;
    }

    /// Set the channel IDs for this channel.
    inline void set_channel_ids(byte_array const& tx_id, byte_array const& rx_id)
    {
        tx_channel_id_ = tx_id;
        rx_channel_id_ = rx_id;
    }

    /**
     * May be called by upper-level protocols during receive
     * to indicate that the packet has been received and processed,
     * so that subsequently transmitted packets include this ack info.
     * if 'send_ack' is true, make sure an acknowledgment gets sent soon:
     * in the next transmitted packet, or in an ack packet if needed.
     */
    void acknowledge(packet_seq_t pktseq, bool send_ack);

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
    inline uia::comm::socket::status link_status() const {
        return link_status_;
    }

    using link_status_changed_signal = boost::signals2::signal<void (uia::comm::socket::status)>;

    /// Indicates when this channel observes a change in link status.
    link_status_changed_signal on_link_status_changed;

protected:
    /**
     * Transmit a packet across the channel.
     * Caller must leave header_len bytes at the beginning for the header. The packet
     * is armored in-place in the provided byte_array. It is the caller's responsibility
     * to transmit only when flow control says it's OK (may_transmit() returns non-zero)
     * or upon getting on_ready_transmit() signal.
     * Provides in 'packet_seq' the transmit sequence number that was assigned to the packet.
     * Returns true if the transmit was successful, or false if it failed (e.g., due
     * to lack of buffer space); a sequence number is assigned even on failure however.
     */
    bool channel_transmit(byte_array& packet, packet_seq_t& packet_seq);

    /**
     * Main method for upper-layer subclass to receive a packet on a channel.
     * Should return true if the packet was processed and should be acked,
     * or false to silently pretend we never received the packet.
     */
    virtual bool channel_receive(packet_seq_t pktseq, byte_array const& pkt) = 0;

    /**
     * Create and transmit a packet for acknowledgment purposes only.
     * Upper layer may override this if ack packets should contain
     * more than just an empty channel payload.
     */
    virtual bool transmit_ack(byte_array &pkt, packet_seq_t ackseq, int ack_count);

    virtual void acknowledged(packet_seq_t txseq, int npackets, packet_seq_t rxackseq);
    virtual void missed(packet_seq_t txseq, int npackets);
    virtual void expire(packet_seq_t txseq, int npackets);

private:
    void start_retransmit_timer();

    /** @name Internal transmit methods. */
    /**@{*/

    /**
     * Private low-level transmit routine:
     * encrypt, authenticate, and transmit a packet whose cleartext header and data are
     * already fully set up, with a specified ACK sequence/count word.
     * Returns true on success, false on error (e.g., no output buffer space for packet)
     */
    bool transmit(byte_array& packet, uint32_t ack_seq, packet_seq_t& packet_seq, bool is_data);

    /**
     * Transmit ack packet with no extra payload.
     * @param  ackseq    Acknowledge sequence number.
     * @param  ack_count Count of consecutively acknowledged packets.
     * @return           true if sent successfully.
     */
    bool tx_ack(packet_seq_t ackseq, int ack_count);
    void flush_ack();

    /**@}*/

    /**
     * Called by link to dispatch a received packet to this channel.
     * @param msg Incoming encrypted packet.
     * @param src Origin endpoint.
     */
    void receive(boost::asio::const_buffer msg, uia::comm::socket_endpoint const& src) override;

    /// Repeat stall indications but not other link status changes.
    /// XXX hack - maybe "stall severity" or "stall time" should be part of status?
    /// Or perhaps status should be (up, stalltime)?
    inline void set_link_status(uia::comm::socket::status new_status) {
        if (link_status_ != new_status or new_status == uia::comm::socket::status::stalled) {
            link_status_ = new_status;
            on_link_status_changed(new_status);
        }
    }

    //-------------------------------------------
    // Handlers
    //-------------------------------------------

    void retransmit_timeout(bool failed); ///< Retransmission timeout
    void ack_timeout();                   ///< Delayed ACK timeout
    void stats_timeout();                 ///< Stats gathering
};

} // sss namespace
