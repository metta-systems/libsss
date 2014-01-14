//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include "byte_array.h"
#include "ssu/link.h"
#include "ssu/link_channel.h"
#include "ssu/channel_armor.h"

namespace ssu {

class host;

/**
 * Abstract base class representing a channel between a local link and a remote endpoint.
 */
class channel : public link_channel
{
    friend class base_stream; // @fixme *sigh*

    typedef link_channel super;

    class private_data;
    std::unique_ptr<private_data> pimpl_;  ///< Most of the state is hidden from interface.

    std::unique_ptr<channel_armor> armor_;         ///< Armors cannot be shared.
    /// Per-direction unique channel IDs for this channel.
    /// Stream layer uses these in assigning USIDs to new streams.
    byte_array   tx_channel_id_;                   ///< Transmit ID of the channel.
    byte_array   rx_channel_id_;                   ///< Receive ID of the channel.
    link::status link_status_{link::status::down}; ///< Link online status.

    static constexpr packet_seq_t max_packet_sequence = ~0ULL;

public:
    /**
     * Amount of space client must leave at the beginning of a packet
     * to be transmitted with channel_transmit() or received via channel_receive().
     * @fixme won't always be static const.
     *
     * Channel header consists of
     * +--------------------------------+-------------------------+
     * | 24-31: Channel number          | 0-23: Transmit sequence | 4 bytes first header word
     * +-------------+------------------+-------------------------+
     * | 28-31: RSVD | 24-27: ACK count | 0-23: ACK sequence      | 4 bytes second header word
     * +-------------+------------------+-------------------------+
     */
    static constexpr int header_len = 8;

    // Layout of the first header word: channel number, tx sequence
    // Transmitted in cleartext.
    static uint32_t make_first_header_word(channel_number channel, uint32_t tx_sequence);

    // Layout of the second header word: ACK count, ACK sequence number
    // Encrypted for transmission.
    static uint32_t make_second_header_word(uint8_t ack_count, uint32_t ack_sequence);

public:
    channel(std::shared_ptr<host> host);
    virtual ~channel();

    virtual std::shared_ptr<host> get_host();

    /// Start the channel.
	void start(bool initiate) override;
    /// Stop the channel.
	void stop() override;

    /// Check congestion control state and return the number of new packets,
    /// if any, that flow control says we may transmit now.
    virtual int may_transmit();

    inline byte_array tx_channel_id() { return tx_channel_id_; }
    inline byte_array rx_channel_id() { return rx_channel_id_; }

    /// Set the channel IDs for this channel.
    inline void set_channel_ids(byte_array const& tx_id, byte_array const& rx_id) {
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
    void acknowledge(uint16_t pktseq, bool send_ack);


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

    typedef boost::signals2::signal<void (link::status)> link_status_changed_signal;

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
    void receive(const byte_array& msg, const link_endpoint& src) override;

    /// Repeat stall indications but not other link status changes.
    /// XXX hack - maybe "stall severity" or "stall time" should be part of status?
    /// Or perhaps status should be (up, stalltime)?
    inline void set_link_status(link::status new_status) {
        if (link_status_ != new_status or new_status == link::status::stalled) {
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

} // namespace ssu
