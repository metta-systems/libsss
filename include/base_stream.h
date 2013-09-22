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
#include <boost/signals2/signal.hpp>
#include "abstract_stream.h"
#include "channel.h"

namespace ssu {

class base_stream;
class stream_channel;

/**
 * Helper representing an attachment point on a stream where the stream attaches to a channel.
 */
class stream_attachment : public stream_protocol
{
public:
    base_stream*     stream_{0};    ///< Our stream.
    stream_channel*  channel_{0};   ///< Channel our stream is attached to.
    stream_id_t      stream_id_{0}; ///< Our stream ID in this channel.
    packet_seq_t     sid_seq_{~0ULL};   ///< Reference packet sequence for stream ID.
};

class stream_tx_attachment : public stream_attachment
{
    bool active_{false};     ///< Currently active and usable.
    bool deprecated_{false}; ///< Opening a replacement channel.

public:
    inline bool is_in_use()       const { return channel_ != nullptr; }
    inline bool is_acknowledged() const { return sid_seq_ != -1ULL; }// todo fixme magic value
    inline bool is_active()       const { return active_; }
    inline bool is_deprecated()   const { return deprecated_; }

    /**
     * Transition from Unused to Attaching -
     * this happens when we send a first Init, Reply, or Attach packet.
     */
    void set_attaching(stream_channel* channel, stream_id_t sid);

    /**
     * Transition from Attaching to Active -
     * this happens when we get an Ack to our Init, Reply, or Attach.
     */
    inline void set_active(packet_seq_t rxseq) {
        assert(is_in_use() && !is_acknowledged());
        sid_seq_ = rxseq;
        active_ = true;
    }

    // Transition to the unused state.
    void clear();
};

class stream_rx_attachment : public stream_attachment
{
public:
    inline bool is_active() const { return channel_ != nullptr; }

    // Transition from unused to active.
    void set_active(stream_channel* channel, stream_id_t sid, packet_seq_t rxseq);

    // Transition to the unused state.
    void clear();
};

/**
 * @internal
 * Basic internal implementation of the abstract stream.
 * The separation between the internal stream control object and the
 * application-visible stream object is primarily needed so that SSU can
 * hold onto a stream's state and gracefully shut it down after the
 * application deletes its stream object representing it.
 * This separation also keeps the internal stream control variables out of the
 * public C++ API header files and thus able to change without breaking binary
 * compatibility, and makes it easy to implement service/protocol negotiation
 * for top-level application streams by extending this class.
 */
class base_stream : public abstract_stream, public std::enable_shared_from_this<base_stream>
{
    friend class stream_channel;

    enum class state {
        created = 0,   ///< Newly created.
        wait_service,  ///< Initiating, waiting for service reply.
        accepting,     ///< Accepting, waiting for service request.
        connected,     ///< Connection established.
        disconnected   ///< Connection terminated.
    };

    /**
     * @internal
     * Unit of data transmission on SSU stream.
     */
    struct packet
    {
        base_stream* owner{nullptr};            ///< Packet owner.
        uint64_t tx_byte_seq{0};                ///< Logical byte position.
        byte_array buf;                         ///< Packet buffer including headers.
        int header_len{0};                      ///< Size of channel and stream headers.
        packet_type type{packet_type::invalid}; ///< Type of this packet.
        bool late{false};                       ///< Possibly lost packet.

        inline packet() = default;
        inline packet(base_stream* o, packet_type t)
            : owner(o)
            , type(t)
        {}
        inline bool is_null() const {
            return owner == nullptr;
        }
        inline int payload_size() const {
            return buf.size() - header_len;
        }

        template <typename T>
        T* header()
        {
            header_len = channel::header_len + sizeof(T);
            buf.resize(header_len);
            return reinterpret_cast<T*>(buf.data() + channel::header_len);
        }
    };

    std::weak_ptr<base_stream> parent_; ///< Parent, if it still exists.
    state state_{state::created};
    uint8_t receive_window_byte_{0};
    bool init_{true};
    bool top_level_{false}; ///< This is a top-level stream.
    bool end_write_{false}; ///< Stream has closed for writing.

    std::queue<packet> tx_queue_; ///< Transmit packets queue.

    unique_stream_id_t usid_,        ///< Unique stream ID.
                       parent_usid_; ///< Unique ID of parent stream.

    // Channel attachment state
    static constexpr int  max_attachments = 2;
    stream_tx_attachment  tx_attachments_[max_attachments];  // Our channel attachments
    stream_rx_attachment  rx_attachments_[max_attachments];  // Peer's channel attachments
    stream_tx_attachment* tx_current_attachment_{0};         // Current transmit-attachment

    static const size_t default_rx_buffer_size = 65536;

    stream_peer* peer_;             ///< Information about the other side of this connection.

    //-------------------------------------------
    // Byte transmit state
    //-------------------------------------------

    /// Next transmit byte sequence number to assign.
    int32_t tx_byte_seq_{0};
    /// Current transmit window.
    int32_t tx_window_{0};
    /// Bytes currently in flight.
    int32_t tx_inflight_{0};
    /// We're enqueued for transmission on our channel.
    bool tx_enqueued_channel_{false};
    /// Segments waiting to be ACKed.
    std::unordered_set<int32_t> tx_waiting_ack_;
    /// Cumulative size of all segments waiting to be ACKed.
    size_t tx_waiting_size_{0};

    // Substream receive state
    /// Received, waiting substreams.
    std::queue<abstract_stream*> received_substreams_;

private:
    void recalculate_receive_window();
    void recalculate_transmit_window();

    inline uint8_t receive_window_byte() const {
        return receive_window_byte_;
    }

    bool is_attached();
    void attach_for_transmit();

    void set_usid(unique_stream_id_t new_usid);

    //-------------------------------------------
    // Transmit various types of packets.
    //-------------------------------------------

    void tx_enqueue_packet(packet& p);
    void tx_enqueue_channel(bool tx_immediately = false);

    /**
     * Send the stream attach packet to the peer.
     */
    void tx_attach();

    /**
     * Send the stream reset packet to the peer.
     */
    static void tx_reset(stream_channel* channel, stream_id_t sid, uint8_t flags);

    /**
     * Called by stream_channel::got_ready_transmit() to transmit or retransmit
     * one packet on a given channel. That packet might have to be an attach packet
     * if we haven't finished attaching yet, or it might have to be an empty segment
     * if we've run out of transmit window space but our latest receive window update
     * may be out-of-date.
     */
    void transmit_on(stream_channel* channel);

    //-------------------------------------------
    // Receive handling for various types of packets
    //-------------------------------------------

    // Returns true if received packet needs to be acked, false otherwise.
    static bool receive(packet_seq_t pktseq, byte_array const& pkt, stream_channel* channel);
    static bool rx_init_packet(packet_seq_t pktseq, byte_array const& pkt, stream_channel* channel);
    static bool rx_reply_packet(packet_seq_t pktseq, byte_array const& pkt, stream_channel* channel);
    static bool rx_data_packet(packet_seq_t pktseq, byte_array const& pkt, stream_channel* channel);
    static bool rx_datagram_packet(packet_seq_t pktseq, byte_array const& pkt, stream_channel* channel);
    static bool rx_ack_packet(packet_seq_t pktseq, byte_array const& pkt, stream_channel* channel);
    static bool rx_reset_packet(packet_seq_t pktseq, byte_array const& pkt, stream_channel* channel);
    static bool rx_attach_packet(packet_seq_t pktseq, byte_array const& pkt, stream_channel* channel);
    static bool rx_detach_packet(packet_seq_t pktseq, byte_array const& pkt, stream_channel* channel);
    void rx_data(byte_array const& pkt, uint32_t byte_seq);

    base_stream* rx_substream(packet_seq_t pktseq, stream_channel* channel,
        stream_id_t sid, unsigned slot, unique_stream_id_t const& usid);

    //-------------------------------------------
    // Signal handlers.
    //-------------------------------------------

    void channel_connected();
    void parent_attached();
    void substream_read_message();

public:
    /**
     * Create a base_stream instance.
     * @param host parent host
     * @param peer the endpoint identifier (EID) of the remote host with which this stream
     *        will be used to communicate. The destination may be either a cryptographic EID
     *        or a non-cryptographic legacy address as defined by the Ident class.
     * @param parent the parent stream, or NULL if none (yet).
     */
    base_stream(std::shared_ptr<host> h, peer_id const& peer, std::shared_ptr<base_stream> parent);
    virtual ~base_stream();

    void fail(std::string const& error);

    /**
     * Connect to a given service on a remote host.
     * @param service the service name to connect to on the remote host.
     *      This parameter replaces the port number
     *      that TCP traditionally uses to differentiate services.
     * @param protocol the application protocol name to connect to.
     */
    void connect_to(std::string const& service, std::string const& protocol);
    void disconnect();

    size_t bytes_available() const override;
    bool at_end() const override; //XXX QIODevice relic
    ssize_t read_data(char* data, size_t max_size) override;
    int pending_records() const override;
    ssize_t read_record(char* data, size_t max_size) override;
    byte_array read_record(size_t max_size) override;
    ssize_t write_data(const char* data, size_t size, uint8_t endflags) override;
    ssize_t read_datagram(char* data, size_t max_size) override;
    ssize_t write_datagram(const char* data, size_t size, stream::datagram_type is_reliable) override;
    byte_array read_datagram(size_t max_size) override;
    abstract_stream* open_substream() override;
    abstract_stream* accept_substream() override;
    bool is_link_up() const override;
    void shutdown(stream::shutdown_mode mode) override;
    void set_receive_buffer_size(size_t size) override;
    void set_child_receive_buffer_size(size_t size) override;
    void dump() override;

    //-------------------------------------------
    // Signals
    //-------------------------------------------

    /**
     * A complete message has been received.
     */
    boost::signals2::signal<void()> on_ready_read_message;

    /**
     * An active attachment attempt succeeded and was acked by receiver.
     */
    boost::signals2::signal<void()> on_attached;

    /**
     * An active detachment attempt succeeded and was acked by receiver.
     */
    boost::signals2::signal<void()> on_detached;
};

} // namespace ssu
