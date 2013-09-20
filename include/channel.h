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
#include "byte_array.h"
#include "link.h"
#include "link_channel.h"
#include "channel_armor.h"
#include "timer.h"
#include "logging.h" // for transmit_event_t debug

namespace ssu {

class host;

/**
 * Abstract base class representing a channel between a local Socket and a remote endpoint.
 */
class channel : public link_channel
{
    friend class base_stream; // @fixme *sigh*

    typedef link_channel super;

    std::shared_ptr<host>          host_;
    std::unique_ptr<channel_armor> armor_; ///< Armors cannot be shared.

    // Retransmit state
    async::timer retransmit_timer_;  ///< Retransmit timer.
    link::status link_status_;       ///< Link online status.

    byte_array   tx_channel_id_;
    byte_array   rx_channel_id_;

    struct transmit_event_t
    {
        int32_t size_;   ///< Total size of packet including hdr
        bool    data_;   ///< Was an upper-layer data packet
        bool    pipe_;   ///< Currently counted toward transmit_data_pipe

        inline transmit_event_t(int32_t size, bool is_data)
            : size_(size)
            , data_(is_data)
            , pipe_(is_data)
        {
            logger::debug() << "New transmission event for " << size_ << (data_ ? " data bytes" : " control bytes");
        }
    };

    static constexpr packet_seq_t max_packet_sequence = ~0ULL;

    // Transmit state
    packet_seq_t tx_sequence_{1};             ///< Next sequence number to transmit
    std::queue<transmit_event_t> tx_events_; ///< Record of transmission events (XX data sizes)
    packet_seq_t tx_event_sequence_{0};       ///< Seqno of oldest recorded tx event
    packet_seq_t tx_ack_sequence_{0};         ///< Highest transmit sequence number ACK'd
    // uint64_t recovseq;   ///< Sequence at which fast recovery finishes
    packet_seq_t mark_sequence_{1};           ///< Transmit sequence number of "marked" packet
    packet_seq_t mark_base_{0};               ///< Snapshot of txackseq at time mark was placed
    boost::posix_time::ptime mark_time_;      ///< Time at which marked packet was sent
    // uint32_t txackmask;  ///< Mask of packets transmitted and ACK'd
    uint32_t tx_inflight_count_{0};       ///< Data packets currently in flight
    uint32_t tx_inflight_size_{0};        ///< Data bytes currently in flight
    uint32_t mark_acks_{0};               ///< Number of ACK'd packets since last mark
    uint32_t mark_sent_{0};               ///< Number of ACKs expected after last mark
    // uint32_t cwnd;       ///< Current congestion window
    // bool cwndlim;       ///< We were cwnd-limited this round-trip

    // Receive state
    // quint64 rxseq;      ///< Highest sequence number received so far
    // quint32 rxmask;     ///< Mask of packets received so far

    // Receive-side ACK state
    packet_seq_t rx_ack_sequence_{0};         ///< Highest sequence number acknowledged so far
    // //quint32 rxackmask;  // Mask of packets received & acknowledged
    packet_seq_t rx_ack_count_{0};             ///< Number of contiguous packets received before rxackseq
    // quint8 rxunacked;   ///< Number of contiguous packets not yet ACKed
    // bool delayack;      ///< Enable delayed acknowledgments
    // Timer acktimer;     ///< Delayed ACK timer

    // Channel statistics.
    async::timer::duration_type cumulative_rtt_;

public:
    /**
     * Amount of space client must leave at the beginning of a packet
     * to be transmitted with channel_transmit() or received via channel_receive().
     * @fixme won't always be static const.
     */
    static constexpr int header_len = 8/*XXX*/;

    // Layout of the first header word: channel number, tx sequence
    // Transmitted in cleartext.
    static uint32_t make_first_header_word(channel_number channel, uint32_t tx_sequence);

    // Layout of the second header word: ACK count, ACK sequence number
    // Encrypted for transmission.
    static uint32_t make_second_header_word(uint8_t ack_count, uint32_t ack_sequence);

public:
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
     * Transmit a packet across the channel.
     * Caller must leave header_len bytes at the beginning for the header. The packet
     * is armored in-place in the provided byte_array. It is the caller's responsibility
     * to transmit only when flow control says it's OK (may_transmit() returns true)
     * or upon getting on_ready_transmit() signal.
     * Provides in 'packet_seq' the transmit sequence number that was assigned to the packet.
     * Returns true if the transmit was successful, or false if it failed (e.g., due
     * to lack of buffer space); a sequence number is assigned even on failure however.
     */
    bool channel_transmit(byte_array& packet, packet_seq_t& packet_seq);

    /**
     * Main method for upper-layer subclass to receive a packet on a flow.
     * Should return true if the packet was processed and should be acked,
     * or false to silently pretend we never received the packet.
     */
    virtual bool channel_receive(packet_seq_t pktseq, byte_array const& pkt) = 0;

    /**
     * Create and transmit a packet for acknowledgment purposes only.
     * Upper layer may override this if ack packets should contain
     * more than an just an empty channel payload.
     */
    virtual bool transmit_ack(byte_array &pkt, packet_seq_t ackseq, unsigned ackct);

    virtual void acknowledged(packet_seq_t txseq, int npackets, packet_seq_t rxackseq);
    virtual void missed(packet_seq_t txseq, int npackets);
    virtual void expire(packet_seq_t txseq, int npackets);

private:
    void reset_congestion_control();

    void start_retransmit_timer();

    /**
     * Private low-level transmit routine:
     * encrypt, authenticate, and transmit a packet whose cleartext header and data are
     * already fully set up, with a specified ACK sequence/count word.
     * Returns true on success, false on error (e.g., no output buffer space for packet)
     */
    bool transmit(byte_array& packet, uint32_t ack_seq, packet_seq_t& packet_seq, bool is_data);

    /**
     * Called by link to dispatch a received packet to this channel.
     * @param msg [description]
     * @param src [description]
     */
    void receive(const byte_array& msg, const link_endpoint& src) override;
};

} // namespace ssu
