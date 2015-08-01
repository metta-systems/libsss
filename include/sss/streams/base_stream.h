//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2015, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <deque>
#include <boost/signals2/signal.hpp>
#include <boost/circular_buffer.hpp>
#include "sss/streams/abstract_stream.h"
#include "sss/channels/channel.h"
#include "sss/internal/usid.h"

namespace sss {

class base_stream;
class stream_channel;

using byte_seq_t = uint64_t;

/**
 * Helper representing an attachment point on a stream where the stream attaches to a channel.
 */
class stream_attachment : public stream_protocol
{
public:
    base_stream*      stream_{nullptr};  ///< Our stream.
    stream_channel*   channel_{nullptr}; ///< Channel our stream is attached to.
    local_stream_id_t stream_id_{0};     ///< Our stream ID in this channel.
    packet_seq_t      sid_seq_{~0ULL};   ///< Reference packet sequence for stream ID.
};

/**
 * Helper class for transmit attachments.
 */
class stream_tx_attachment : public stream_attachment
{
    bool active_{false};     ///< Currently active and usable.
    bool deprecated_{false}; ///< Opening a replacement channel.

public:
    inline bool is_in_use()       const { return channel_ != nullptr; }
    inline bool is_acknowledged() const { return sid_seq_ != ~0ULL; }// todo fixme magic value
    inline bool is_active()       const { return active_; }
    inline bool is_deprecated()   const { return deprecated_; }

    /**
     * Transition from Unused to Attaching -
     * this happens when we send a first STREAM_ATTACH frame.
     */
    void set_attaching(stream_channel* channel, local_stream_id_t sid);

    /**
     * Transition from Attaching to Active -
     * this happens when we get an ACK frame to our STREAM_ATTACH.
     */
    inline void set_active(packet_seq_t rxseq)
    {
        assert(is_in_use() and not is_acknowledged());
        sid_seq_ = rxseq;
        active_ = true;
    }

    // Transition to the unused state.
    void clear();
};

/**
 * Helper class for receive attachments.
 */
class stream_rx_attachment : public stream_attachment
{
public:
    inline bool is_active() const { return channel_ != nullptr; }

    // Transition from Unused to Active.
    void set_active(stream_channel* channel, local_stream_id_t sid, packet_seq_t rxseq);

    // Transition to the Unused state.
    void clear();
};

/**
 * @nosubgrouping
 *
 * Basic internal implementation of the abstract stream.
 * The separation between the internal stream control object and the
 * application-visible stream object is primarily needed so that SSS can
 * hold onto a stream's state and gracefully shut it down after the
 * application deletes its stream object representing it.
 * This separation also keeps the internal stream control variables out of the
 * public C++ API header files and thus able to change without breaking binary
 * compatibility, and makes it easy to implement service/protocol negotiation
 * for top-level application streams by extending this class.
 */
class base_stream : public abstract_stream, public std::enable_shared_from_this<base_stream>
{
    using super = abstract_stream;

    friend class stream; // access to self_...
    friend class stream_channel;
    friend class stream_tx_attachment; // access to tx_current_attachment_

    enum class state {
        created = 0,   ///< Newly created.
        wait_service,  ///< Initiating, waiting for service reply.
        accepting,     ///< Accepting, waiting for service request.
        connected,     ///< Connection established.
        disconnected   ///< Connection terminated.
    };

    //=============================================================================================
    // Helper types
    //=============================================================================================

    /**
     * User-writable stream data storage.
     * ACKed buffer segments are freed. When a contiguous chunk is ACKed, buffer is advanced to
     * allow writing more.
     */
    boost::circular_buffer<uint8_t> buffer_; // bounded_buffer here instead

    /**
     * @internal
     * Unit of data transmission on SSS stream.
     * Frame represents a segment inside buffer that is being transmitted.
     * Frame's logical byte position refers to position within full stream,
     * they are used for ACKing.
     * Frame's range covers [tx_byte_seq_ .. tx_byte_seq_ + buffer_size(payload_)]
     * Frames are assembled via framing layer.
     */
    struct tx_frame_t
    {
        base_stream* owner{nullptr};            ///< Frame owner.
        byte_seq_t tx_byte_seq_{0};             ///< Transmit byte position within stream.
        boost::asio::const_buffer payload_;            ///< Frame data.
        bool late{false};                       ///< Possibly lost frame.

        inline tx_frame_t() = default;
        inline tx_frame_t(base_stream* o, bool is_fec)
            : owner(o)
            // , FEC(is_fec)
        {}
        inline frame_type type() const {
            return frame_type::EMPTY;
        }
        inline bool is_null() const {
            return owner == nullptr;
        }
        inline int payload_size() const {
            return boost::asio::buffer_size(payload_);
        }

        // template <typename T>
        // inline T* header()
        // {
        //     header_len = channel::header_len + sizeof(T);
        //     if (payload.size() < size_t(header_len)) {
        //         payload.resize(header_len);
        //     }
        //     return boost::asio::buffer_cast<T*>(payload_);
        // }

        // template <typename T>
        // inline T const* header() const {
        //     return boost::asio::buffer_cast<T const*>(payload_);
        // }
    };
    friend std::ostream& operator << (std::ostream& os, tx_frame_t const& frame);

    /**
     * @internal
     * Description of received data segment.
     */
    struct rx_segment_t
    {
        byte_seq_t rx_byte_seq_{0}; ///< Logical byte position.
        byte_array buf;         ///< Packet buffer including headers.
        bool end_marker_{false};///< This segment is end of record.

        inline rx_segment_t(byte_array const& data, byte_seq_t rx_seq, bool end)
            : rx_byte_seq_(rx_seq)
            , buf(data)
            , end_marker_(end)
        {}

        inline int segment_size() const {
            return buf.size();
        }

        inline bool is_record_end() const {
            return end_marker_;
        }
    };

    //=============================================================================================
    /** @name Connection state */
    //=============================================================================================
    /**@{*/

    std::weak_ptr<base_stream> parent_; ///< Parent, if it still exists.
    /**
     * Self-reference to keep this stream around until it is done.
     * It is initialized from stream::connect_to() or stream private constructor.
     * @fixme This is nasty, think of a better implementation.
     */
    std::shared_ptr<base_stream> self_;
    state state_{state::created};
    bool init_{true};       ///< Starting a new stream and its attach hasn't been acknowledged yet.
    bool top_level_{false}; ///< This is a top-level stream.
    bool end_read_{false};  ///< Seen or forced EOF for reading.
    bool end_write_{false}; ///< We've written EOF marker.

    unique_stream_id_t usid_,        ///< Unique stream ID.
                       parent_usid_; ///< Unique ID of parent stream.
    internal::stream_peer* peer_;    ///< Information about the other side of this connection.

    /**@}*/
    //=============================================================================================
    /** @name Channel attachment state */
    //=============================================================================================
    /**@{*/

    /// Stream is usually connected to a single channel via it's 0 index attachment point.
    /// When migrating from old to new channel, stream may be connected to two channels at once,
    /// the new channel being connected via attachment index 1.

    static constexpr int  max_attachments = 2;
    stream_tx_attachment  tx_attachments_[max_attachments];  // Our channel attachments
    stream_rx_attachment  rx_attachments_[max_attachments];  // Peer's channel attachments
    stream_tx_attachment* tx_current_attachment_{0};         // Current transmit-attachment

    /**@}*/
    //=============================================================================================
    /** @name Byte transmit state */
    //=============================================================================================
    /**@{*/

    byte_seq_t tx_byte_seq_{0}; ///< Next transmit byte sequence number to assign.
    int32_t tx_window_{0}; ///< Current transmit window.
    int32_t tx_inflight_{0}; ///< Bytes currently in flight.
    bool tx_enqueued_channel_{false}; ///< We're enqueued for transmission on our channel.
    std::unordered_set<byte_seq_t> tx_waiting_ack_; ///< Frames waiting to be ACKed.
    std::deque<tx_frame_t> tx_queue_; ///< Transmit frames queue.
    size_t tx_waiting_size_{0}; ///< Cumulative size of all segments waiting to be ACKed.

    /**@}*/
    //=============================================================================================
    /** @name Byte receive state */
    //=============================================================================================
    /**@{*/

    /// Default receive buffer size for new top-level streams
    static constexpr int default_rx_buffer_size = 65536;

    /// Next SSN expected to arrive.
    byte_seq_t rx_byte_seq_{0};
    /// Received bytes available.
    int32_t rx_available_{0};
    /// Bytes avail in current message.
    int32_t rx_record_available_{0};
    /// Total buffer space used.
    int32_t rx_buffer_used_{0};
    /// Receive window log2.
    uint8_t receive_window_byte_{0};

    /// Received out of order.
    std::deque<rx_segment_t> readahead_;
    /// Received, waiting to be read.
    std::deque<rx_segment_t> rx_segments_;
    /// Sizes of received messages.
    std::deque<ssize_t> rx_record_sizes_;

    int receive_buf_size_{default_rx_buffer_size};       // Recv buf size for channel control
    int child_receive_buf_size_{default_rx_buffer_size}; // Recv buf for child streams

    /**@}*/
    //=============================================================================================
    /** @name Substream receive state */
    //=============================================================================================
    /**@{*/

    /// Received, waiting substreams.
    std::deque<std::shared_ptr<abstract_stream>> received_substreams_;
    /// Received, waiting datagram streams.
    std::deque<std::shared_ptr<abstract_stream>> received_datagrams_;

    /**@}*/
private:
    // Connection
    void got_service_request();
    void got_service_reply();

    bool is_attached();
    // Actively initiate a transmit-attachment
    void attach_for_transmit();

    void set_usid(unique_stream_id_t new_usid);

    //=============================================================================================
    // Transmit various types of packets.
    //=============================================================================================

    void tx_enqueue_packet(tx_frame_t& p);
    void tx_enqueue_channel(bool tx_immediately = false);

    /**
     * Send the stream attach packet to the peer.
     */
    void tx_attach();

    void tx_attach_data(frame_type type, local_stream_id_t ref_sid);
    void tx_data(tx_frame_t& p);
    void tx_datagram();

    /**
     * Send the stream reset packet to the peer.
     */
    static void tx_reset(stream_channel* channel, local_stream_id_t sid, uint8_t flags);

    /**
     * Called by stream_channel::got_ready_transmit() to transmit or retransmit
     * one packet on a given channel. That packet might have to be an attach packet
     * if we haven't finished attaching yet, or it might have to be an empty segment
     * if we've run out of transmit window space but our latest receive window update
     * may be out-of-date.
     */
    void transmit_on(stream_channel* channel);

    //=============================================================================================
    // Receive handling for various types of packets
    // @todo Types are now different and they are FRAMES within the packet, so framing layer
    // should dispatch them, not stream.
    //=============================================================================================

    // Returns true if received packet needs to be acked, false otherwise.
    static bool receive(packet_seq_t pktseq, byte_array const& pkt, stream_channel* channel);

    bool rx_reset_frame(boost::asio::const_buffer pkt);
    bool rx_priority_frame(boost::asio::const_buffer pkt);
    bool rx_stream_frame(boost::asio::const_buffer pkt);
    bool rx_detach_frame(boost::asio::const_buffer pkt);

    // composite callback from channel
    bool rx_ack_frame(byte_seq_t byteseq, size_t size);

    static bool rx_init_packet(packet_seq_t pktseq, byte_array const& pkt,
        stream_channel* channel);
    static bool rx_reply_packet(packet_seq_t pktseq, byte_array const& pkt,
        stream_channel* channel);
    static bool rx_data_packet(packet_seq_t pktseq, byte_array const& pkt,
        stream_channel* channel);
    static bool rx_datagram_packet(packet_seq_t pktseq, byte_array const& pkt,
        stream_channel* channel);
    static bool rx_ack_packet(packet_seq_t pktseq, byte_array const& pkt,
        stream_channel* channel);
    static bool rx_reset_packet(packet_seq_t pktseq, byte_array const& pkt,
        stream_channel* channel);
    static bool rx_attach_packet(packet_seq_t pktseq, byte_array const& pkt,
        stream_channel* channel);
    static bool rx_detach_packet(packet_seq_t pktseq, byte_array const& pkt,
        stream_channel* channel);
    void rx_data(byte_array const& pkt, uint32_t byte_seq);

    std::shared_ptr<base_stream> rx_substream(packet_seq_t pktseq, stream_channel* channel,
        local_stream_id_t sid, unsigned slot, unique_stream_id_t const& usid);

    // Helper function to enqueue useful rx segment data.
    void rx_enqueue_segment(rx_segment_t const& seg, size_t actual_size, bool& closed);

    // Return the next receive window update byte
    // for some packet we are transmitting on this stream.
    // XX alternate between byte-window and substream-window updates.
    inline uint8_t receive_window_byte() const {
        return receive_window_byte_;
    }

    void recalculate_receive_window();
    void recalculate_transmit_window(uint8_t window_byte);

    //=============================================================================================
    // Signal handlers.
    //=============================================================================================

    /**
     * We connect this signal to our stream_peer's on_channel_connected()
     * while waiting for a channel to attach to.
     */
    void channel_connected();

    /**
     * We connect this signal to our parent stream's on_attached() signal
     * while we're waiting for it to attach so we can init.
     */
    void parent_attached();

    /**
     * We connect this to the on_ready_read_record() signals
     * of any substreams queued in our received_substreams_ list waiting to be accepted,
     * in order to forward the indication to the client
     * via the parent stream's on_ready_read_datagram() signal.
     *
     * Originally in SST this was used to indicate reliable or too large datagrams sent as
     * substreams to be completed and ready to receive.
     * Now we need to find a better way to indicate that. Ephemeral stream kind of flag.
     */
    // void substream_read_record();

public:
    /**
     * Create a base_stream instance.
     * @param host parent host
     * @param peer the endpoint identifier (EID) of the remote host with which this stream
     *        will be used to communicate. The destination may be either a cryptographic EID
     *        or a non-cryptographic legacy address as defined by the Ident class.
     * @param parent the parent stream, or nullptr if none (yet).
     */
    base_stream(std::shared_ptr<host> h, uia::peer_identity const& peer,
        std::shared_ptr<base_stream> parent);
    virtual ~base_stream();

    //=============================================================================================
    // Stream online status.
    //=============================================================================================

    /**
     * Connect to a given service on a remote host.
     * @param service the service name to connect to on the remote host.
     *      This parameter replaces the port number
     *      that TCP traditionally uses to differentiate services.
     * @param protocol the application protocol name to connect to.
     */
    void connect_to(std::string const& service, std::string const& protocol);

    /**
     * Immediately reset a stream to the disconnected state.
     * Outstanding buffered data may be lost.
     */
    void disconnect();

    // Clear out this stream's state as if preparing for deletion,
    // without actually deleting the object yet.
    void clear();

    /**
     * Disconnect and set an error condition.
     * @param error Error message.
     */
    void fail(std::string const& error);

    /**
     * Returns true if the underlying link is currently connected and usable for data transfer.
     */
    inline bool is_link_up() const override {
        return state_ == state::connected;
    }

    void shutdown(stream::shutdown_mode mode) override;

    //=============================================================================================
    // Reading and writing application data.
    //=============================================================================================

    ssize_t bytes_available() const override;
    bool at_end() const override;

    inline int pending_records() const override {
        return rx_record_sizes_.size();
    }

    inline ssize_t pending_record_size() const override {
        return has_pending_records() ? rx_record_sizes_.front() : -1;
    }

    ssize_t read_record(char* data, ssize_t max_size) override;
    byte_array read_record(ssize_t max_size) override;

    ssize_t read_data(char* data, ssize_t max_size) override;
    ssize_t write_data(char const* data, ssize_t size, uint8_t endflags) override;

    //=============================================================================================
    // Substreams.
    //=============================================================================================

    // Initiate or accept substreams
    std::shared_ptr<abstract_stream> open_substream() override;
    std::shared_ptr<abstract_stream> accept_substream() override;

    // Send and receive unordered, unreliable datagrams on this stream.
    std::shared_ptr<abstract_stream> get_datagram();
    ssize_t read_datagram(char* data, ssize_t max_size) override;
    byte_array read_datagram(ssize_t max_size) override;
    ssize_t write_datagram(char const* data, ssize_t size,
        stream::datagram_type is_reliable) override;

    void set_receive_buffer_size(size_t size) override;
    void set_child_receive_buffer_size(size_t size) override;

    /**
     * Dump the state of this stream, for debugging purposes.
     */
    void dump() override;

    /**
     * Set the stream's transmit priority level.
     * This method overrides abstract_stream's default method
     * to move the stream to the correct transmit queue if necessary.
     */
    void set_priority(priority_t priority) override;

    // stream_channel calls these to return our transmitted packets to us
    // after being held in waiting_ack_.
    // The missed() method returns true if the channel should keep track
    // of the packet until it expires, at which point it calls expire()
    // and unconditionally removes it.
    void acknowledged(stream_channel* channel, tx_frame_t const& pkt, packet_seq_t rx_seq);
    bool missed(stream_channel* channel, tx_frame_t const& pkt);
    void expire(stream_channel* channel, tx_frame_t const& pkt);

    void end_flight(tx_frame_t const& pkt);

    //=============================================================================================
    /** @name Signals */
    //=============================================================================================
    /**@{*/

    /**
     * An active attachment attempt succeeded and was acked by receiver.
     */
    boost::signals2::signal<void()> on_attached;

    /**
     * An active detachment attempt succeeded and was acked by receiver.
     */
    boost::signals2::signal<void()> on_detached;
    /**@}*/
};

//=================================================================================================
// Helper functions.
//=================================================================================================

inline std::ostream& operator << (std::ostream& os, sss::base_stream::tx_frame_t const& pkt)
{
    std::string frame_type = [](stream_protocol::frame_type type){
        switch (type) {
            case stream_protocol::frame_type::ACK:          return "ack";
            case stream_protocol::frame_type::CLOSE:        return "close";
            case stream_protocol::frame_type::DECONGESTION: return "decongestion";
            case stream_protocol::frame_type::DETACH:       return "detach";
            case stream_protocol::frame_type::EMPTY:        return "empty";
            case stream_protocol::frame_type::RESET:        return "reset";
            case stream_protocol::frame_type::SETTINGS:     return "settings";
            case stream_protocol::frame_type::STREAM:       return "stream";
            case stream_protocol::frame_type::PADDING:      return "padding";
            case stream_protocol::frame_type::PRIORITY:     return "priority";
            default:                                        return "unknown";
        }
    }(pkt.type());

    os << "[packet txseq " << pkt.tx_byte_seq_ << ", type " << frame_type
       << ", owner " << pkt.owner << (pkt.late ? ", late" : ", not late")
       << ", payload " << pkt.payload_ << "]";
    return os;
}

template <typename T>
inline T const* as_header(byte_array const& v)
{
    return reinterpret_cast<T const*>(v.const_data() + channel::header_len);
}

template <typename T>
inline T* as_header(byte_array& v)
{
    size_t header_len = channel::header_len + sizeof(T);
    if (v.size() < header_len) {
        v.resize(header_len);
    }
    return reinterpret_cast<T*>(v.data() + channel::header_len);
}

} // sss namespace
