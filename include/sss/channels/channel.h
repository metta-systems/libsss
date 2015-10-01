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
#include "sss/forward_ptrs.h"

namespace sss {

class host;

/**
 * Base class for socket-based channels,
 * for dispatching received packets based on endpoint and channel number.
 * May be used as an abstract base by overriding the receive() method,
 * or used as a concrete class by connecting to the on_received signal.
 */
class socket_channel : public std::enable_shared_from_this<socket_channel>
{
    uia::comm::socket_wptr socket_;             ///< Socket we're currently bound to, if any.
    uia::comm::endpoint remote_ep_;  ///< Endpoint of the remote side.
    bool active_{false};             ///< True if we're sending and accepting packets.
    std::string remote_channel_key_; ///< Far end short-term public key.
    std::string local_channel_key_;  ///< Channel key of this channel at local node
                                     ///< (Near end short-term public key).

public:
    inline virtual ~socket_channel() { unbind(); }

    /**
     * Start the channel.
     * @param initiate Initiate the key exchange using kex_initiator.
     */
    inline virtual void start(bool initiate)
    {
        assert(!remote_channel_key_.empty());
        active_ = true;
    }

    /**
     * Stop the channel.
     */
    inline virtual void stop() { active_ = false; }

    inline bool is_active() const { return active_; }

    inline bool is_bound() const { return socket_.lock() != nullptr; }

    /**
     * Test whether underlying socket is already congestion controlled.
     */
    inline bool is_congestion_controlled()
    {
        return socket_.lock()->is_congestion_controlled(remote_ep_);
    }

    /**
     * Return the remote endpoint we're bound to, if any.
     */
    inline uia::comm::socket_endpoint remote_endpoint() const
    {
        return uia::comm::socket_endpoint(socket_, remote_ep_);
    }

    /**
     * Set up for communication with specified remote endpoint,
     * binding to a particular local channel key.
     * @returns false if the channel is already in use and cannot be bound to.
     *
     * @fixme Channel key here is the peer's public key, and this binding should not be to the
     * socket but to the message_receiver.
     * It also should skip remote EP entirely and bind based only on channel key.
     * Sending should be directed to EP from which _the latest_ packet was received from this
     * peer. And as such a lower-level must maintain this channelkey<->ep mapping somewhere.
     * (Current implementation is largely invalid because it uses remote_ep_ as peer address).
     */
    // bool bind(socket::weak_ptr socket, endpoint const& remote_ep, std::string channel_key);

    // inline bool bind(socket_endpoint const& remote_ep, std::string channel_key) {
    //     return bind(remote_ep.socket(), remote_ep, channel_key);
    // }

    /**
     * Stop channel and unbind from any currently bound remote endpoint.
     * This removes cached local and remote short-term public keys, making channel
     * unable to decode and further received packets with these keys. This provides
     * forward secrecy.
     * After unbind() is called no communication may happen over the channel and a new one
     * must be established to continue communication.
     */
    void unbind();

    /**
     * Return current local channel number.
     */
    inline std::string local_channel() const { return local_channel_key_; }

    /**
     * Return current remote channel number.
     */
    inline std::string remote_channel() const { return remote_channel_key_; }

    /**
     * Set the channel number to direct packets to the remote endpoint.
     * This MUST be done before a new channel can be activated.
     */
    inline void set_remote_channel(std::string ch) { remote_channel_key_ = ch; }

    /**
     * Receive a network packet msg from endpoint src.
     * Implementations may override this function or simply connect to on_received() signal.
     * Default implementation simply emits on_received() signal.
     * @param msg A received network packet
     * @param src Sender endpoint
     */
    inline virtual void receive(boost::asio::const_buffer msg,
                                uia::comm::socket_endpoint const& src)
    {
        on_received(msg, src);
    }

    /** @name Signals. */
    /**@{*/
    // Provide access to signal types for clients
    using received_signal =
        boost::signals2::signal<void(boost::asio::const_buffer, uia::comm::socket_endpoint const&)>;
    using ready_transmit_signal = boost::signals2::signal<void()>;

    /**
     * Signalled when channel receives a packet.
     */
    received_signal on_received;
    /**
     * Signalled when channel congestion control may allow new transmission.
     */
    ready_transmit_signal on_ready_transmit;
    /**@}*/

protected:
    /**
     * When the underlying socket is already congestion-controlled, this function returns
     * the number of bytes that channel control says we may transmit now, 0 if none.
     */
    virtual size_t may_transmit();

    /**
     * Send a network packet and return success status.
     * @param  pkt Network packet to send
     * @return     true if socket call succeeded. The packet may actually have not been sent.
     */
    inline bool send(byte_array const& pkt) const
    {
        assert(active_);
        if (auto s = socket_.lock()) {
            return s->send(remote_ep_, pkt);
        }
        return false;
    }
};

/**
 * Abstract base class representing a channel between a local link and a remote endpoint.
 */
class channel : public socket_channel, public std::enable_shared_from_this<channel>
{
    friend class base_stream; // @fixme *sigh*

    using super = socket_channel;

    class private_data;
    std::unique_ptr<private_data> pimpl_; ///< Most of the state is hidden from interface.

    // Packet encode/decode.
    // @todo Move to pimpl

    // @todo change these to krypto::secret_key and krypto::public_key
    sodiumpp::secret_key local_key_;
    sodiumpp::public_key remote_key_;

    /**
     * Encode and authenticate data packet.
     * @param  pkt    Packet to encode.
     * @return        Encoded and authenticated packet.
     */
    byte_array transmit_encode(boost::asio::mutable_buffer pkt);
    /**
     * Decode packet.
     * @param  in     Incoming packet.
     * @param  out    Decrypted packet.
     * @return        true if packet is verified to be authentic and decoded.
     */
    bool receive_decode(boost::asio::const_buffer in, byte_array& out);

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
    static constexpr size_t header_len = 0; // @fixme Get rid of this

    channel(std::shared_ptr<host> host,
            sodiumpp::secret_key local_key,
            sodiumpp::public_key remote);
    virtual ~channel();

    virtual std::shared_ptr<host> get_host();

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

} // sss namespace
