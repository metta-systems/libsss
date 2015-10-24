//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <boost/asio.hpp>
#include "sodiumpp/sodiumpp.h"
#include "arsenal/byte_array.h"
#include "uia/comm/socket.h"
#include "sss/framing/stream_protocol.h"
#include "sss/streams/base_stream.h"
#include "sss/internal/usid.h"
#include "sss/internal/timer.h"
#include "sss/forward_ptrs.h"

namespace sss {
class stream_tx_attachment;
class stream_rx_attachment;
namespace internal {
class stream_peer;
} // internal namespace

/**
 * Abstract base class representing a channel between a local link and a remote endpoint.
 */
class channel : public socket_channel
{
    friend class base_stream; // @fixme *sigh*

    using super = socket_channel;

    class private_data;
    std::unique_ptr<private_data> pimpl_; ///< Most of the state is hidden from interface.

    /// Per-direction unique channel IDs for this channel.
    /// Stream layer uses these in assigning USIDs to new streams.
    byte_array tx_channel_id_; ///< Transmit ID of the channel.
    byte_array rx_channel_id_; ///< Receive ID of the channel.

    uia::comm::socket::status link_status_{
        uia::comm::socket::status::down}; ///< Link online status.

    /**
     * When packet sequence reaches this number, the channel is no longer usable
     * and must be terminated.
     * It is advised to create a new channel long before reaching this limit.
     */
    static constexpr packet_seq_t max_packet_sequence = ~0ULL;

public:
    channel(std::shared_ptr<host> host,
            sodiumpp::secret_key local_key,
            sodiumpp::public_key remote);
    virtual ~channel();

    virtual host_ptr get_host();

    /// Start the channel.
    void start(bool initiate) override;
    /// Stop the channel.
    void stop() override;

    /// Check congestion control state and return the number of new packets,
    /// if any, that flow control says we may transmit now.
    size_t may_transmit() override;

    inline byte_array tx_channel_id() { return tx_channel_id_; }
    inline byte_array rx_channel_id() { return rx_channel_id_; }

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
    // inline void set_armor(std::unique_ptr<channel_armor> armor) {
    // armor_ = std::move(armor);
    // }

    /**
     * Return the current link status as observed by this channel.
     */
    inline uia::comm::socket::status link_status() const { return link_status_; }

    using link_status_changed_signal = boost::signals2::signal<void(uia::comm::socket::status)>;

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
    bool channel_transmit(boost::asio::const_buffer packet, packet_seq_t& packet_seq);

    /**
     * Main method for upper-layer subclass to receive a packet on a channel.
     * Should return true if the packet was processed and should be acked,
     * or false to silently pretend we never received the packet.
     */
    virtual bool channel_receive(boost::asio::mutable_buffer pkt, packet_seq_t packet_seq) = 0;

    /**
     * Create and transmit a packet for acknowledgment purposes only.
     * Upper layer may override this if ack packets should contain
     * more than just an empty channel payload.
     */
    virtual bool transmit_ack(byte_array& pkt, packet_seq_t ackseq, int ack_count);

    virtual void acknowledged(packet_seq_t txseq, int npackets, packet_seq_t rxackseq);
    virtual void missed(packet_seq_t txseq, int npackets);
    virtual void expire(packet_seq_t txseq, int npackets);

private:
    void start_retransmit_timer();

    packet_seq_t derive_packet_seq(packet_seq_t partial_seq);

    /** @name Internal transmit methods. */
    /**@{*/

    /**
     * Private low-level transmit routine:
     * encrypt, authenticate, and transmit a packet whose cleartext header and data are
     * already fully set up, with a specified ACK sequence/count word.
     * Returns true on success, false on error (e.g., no output buffer space for packet)
     */
    bool transmit(boost::asio::const_buffer packet,
                  uint32_t ack_seq,
                  packet_seq_t& packet_seq,
                  bool is_data);

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
     * Called by socket to dispatch a received packet to this channel.
     * @param msg Incoming encrypted packet.
     * @param src Origin endpoint.
     */
    void receive(boost::asio::const_buffer msg, uia::comm::socket_endpoint const& src) override;

    /// Repeat stall indications but not other socket status changes.
    /// XXX hack - maybe "stall severity" or "stall time" should be part of status?
    /// Or perhaps status should be (up, stalltime)?
    inline void set_link_status(uia::comm::socket::status new_status)
    {
        if (link_status_ != new_status or new_status == uia::comm::socket::status::stalled) {
            link_status_ = new_status;
            on_link_status_changed(new_status);
        }
    }

    /**
     * Internal statistics collection.
     * @param src Source endpoint of bad packet.
     */
    void runt_packet_received(uia::comm::socket_endpoint const& src);
    void bad_auth_received(uia::comm::socket_endpoint const& src);

    size_t runt_packets_{0};
    size_t bad_auth_packets_{0};

    //-------------------------------------------
    // Handlers
    //-------------------------------------------

    void retransmit_timeout(bool failed); ///< Retransmission timeout
    void ack_timeout();                   ///< Delayed ACK timeout
    void stats_timeout();                 ///< Stats gathering
};

/**
 * Channel implementation for SSS streams.
 */
class stream_channel : public channel, public stream_protocol
{
    friend class base_stream;          /// @fixme
    friend class stream_rx_attachment; /// @fixme
    friend class stream_tx_attachment; /// @fixme

    using super = channel;

    /**
     * Retry connection attempts for persistent streams once every minute.
     */
    static const async::timer::duration_type connect_retry_period;

    /**
     * Number of stall warnings we get from our primary stream
     * before we start a new lookup/key exchange phase to try replacing it.
     */
    static constexpr int stall_warnings_max = 3;

    int stall_warnings_{0}; ///< Stall warnings before new lookup.

    /**
     * Stream peer this channel is associated with.
     * A stream_channel is always a direct child of its stream_peer
     * so there should be no chance of this pointer ever dangling.
     */
    sss::internal::stream_peer* peer_{nullptr};

    /**
     * Top-level stream used for connecting to services.
     */
    std::shared_ptr<base_stream> root_{nullptr};

    // Hash table of active streams indexed by stream ID
    std::unordered_map<local_stream_id_t, stream_tx_attachment*>
        transmit_sids_; // Our SID namespace
    std::unordered_map<local_stream_id_t, stream_rx_attachment*>
        receive_sids_; // Peer's SID namespace

    counter_t transmit_sid_counter_{1}; // Next stream counter to assign.
    counter_t transmit_sid_acked_{0};   // Last acknowledged stream counter.
    counter_t received_sid_counter_{0}; // Last stream counter received.

    /**
     * Closed stream IDs waiting for close acknowledgment.
     */
    std::unordered_set<local_stream_id_t> closed_streams_;

    /**
     * Streams queued for transmission on this channel.
     * This should be a priority queue for simplicity of enqueueing.
     * We use deque with the limitation that only push_back() and pop_front() are used.
     * Of course insertion into a deque is a bit more involved.
     * @fixme would prefer a smarter scheduling algorithm, e.g., stride.
     */
    std::deque<base_stream*> sending_streams_;

    /**
     * Packets transmitted and waiting for acknowledgment,
     * indexed by assigned transmit sequence number.
     */
    std::unordered_map<packet_seq_t, base_stream::tx_frame_t> waiting_ack_;

    /**
     * Packets already presumed lost ("missed")
     * but still waiting for potential acknowledgment until expiry.
     */
    std::unordered_map<packet_seq_t, base_stream::tx_frame_t> waiting_expiry_;

    /**
     * RxSID of stream on which we last received a packet -
     * this determines for which stream we send receive window info
     * when transmitting "bare" Ack packets.
     */
    local_stream_id_t ack_sid_;

    // Handlers.
    void got_link_status_changed(uia::comm::socket::status new_status);
    void got_ready_transmit();

public:
    stream_channel(std::shared_ptr<host> host,
                   sss::internal::stream_peer* peer,
                   uia::peer_identity const& id);
    ~stream_channel();

    void start(bool initiate) override;
    void stop() override;

    inline sss::internal::stream_peer* target_peer() { return peer_; }
    inline std::shared_ptr<base_stream> root_stream() { return root_; }

    counter_t allocate_transmit_sid();

    void enqueue_stream(base_stream* stream);
    void dequeue_stream(base_stream* stream);

    /**
     * Detach all streams currently transmit-attached to this channel,
     * and send any of their outstanding packets back for retransmission.
     */
    void detach_all();

    /**
     * Override channel's default transmit_ack() method
     * to include stream-layer info in explicit ack packets.
     */
    bool transmit_ack(byte_array& pkt, packet_seq_t ackseq, int ack_count) override;

    void acknowledged(packet_seq_t txseq, int npackets, packet_seq_t rxackseq) override;
    void missed(packet_seq_t txseq, int npackets) override;
    void expire(packet_seq_t txseq, int npackets) override;

    bool channel_receive(boost::asio::mutable_buffer pkt, packet_seq_t packet_seq) override;
};

} // sss namespace
