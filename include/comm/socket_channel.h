//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <boost/signals2/signal.hpp>
#include "arsenal/byte_array.h"
#include "comm/socket_endpoint.h"
#include "comm/socket.h"

namespace uia {
namespace comm {

/**
 * Base class for socket-based channels,
 * for dispatching received packets based on endpoint and channel number.
 * May be used as an abstract base by overriding the receive() method,
 * or used as a concrete class by connecting to the on_received signal.
 */
class socket_channel : std::enable_shared_from_this<socket_channel>
{
    socket::weak_ptr socket_;             ///< Socket we're currently bound to, if any.
    endpoint         remote_ep_;          ///< Endpoint of the remote side.
    bool             active_{false};      ///< True if we're sending and accepting packets.
    std::string      remote_channel_key_; ///< Far end short-term public key.
    std::string      local_channel_key_;  ///< Channel key of this channel at local node
                                          ///< (Near end short-term public key).

public:
    using weak_ptr = std::weak_ptr<socket_channel>;
    using ptr = std::shared_ptr<socket_channel>;

    inline virtual ~socket_channel() {
        unbind();
    }

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
    inline virtual void stop() {
        active_ = false;
    }

    inline bool is_active() const {
        return active_;
    }

    inline bool is_bound()  const {
        return socket_.lock() != nullptr;
    }

    /**
     * Test whether underlying socket is already congestion controlled.
     */
    inline bool is_congestion_controlled() {
        return socket_.lock()->is_congestion_controlled(remote_ep_);
    }

    /**
     * Return the remote endpoint we're bound to, if any.
     */
    inline socket_endpoint remote_endpoint() const {
        return socket_endpoint(socket_, remote_ep_);
    }

    /**
     * Set up for communication with specified remote endpoint,
     * binding to a particular local channel key.
     * @returns false if the channel is already in use and cannot be bound to.
     */
    bool bind(socket::weak_ptr socket, endpoint const& remote_ep, std::string channel_key);

    inline bool bind(socket_endpoint const& remote_ep, std::string channel_key) {
        return bind(remote_ep.socket(), remote_ep, channel_key);
    }

    /**
     * Stop channel and unbind from any currently bound remote endpoint.
     * This removes cached local and remote short-term public keys, making channel
     * unable to decode and further received packets with these keys. This provides
     * forward secrecy.
     */
    void unbind();

    /**
     * Return current local channel number.
     */
    inline std::string local_channel() const {
        return local_channel_key_;
    }

    /**
     * Return current remote channel number.
     */
    inline std::string remote_channel() const {
        return remote_channel_key_;
    }

    /**
     * Set the channel number to direct packets to the remote endpoint.
     * This MUST be done before a new channel can be activated.
     */
    inline void set_remote_channel(std::string ch) {
        remote_channel_key_ = ch;
    }

    /**
     * Receive a network packet msg from endpoint src.
     * Implementations may override this function or simply connect to
     * on_received() signal.
     * @param msg A received network packet
     * @param src Sender endpoint
     */
    inline virtual void receive(byte_array const& msg, socket_endpoint const& src) {
        on_received(msg, src);
    }

    /** @name Signals. */
    /**@{*/
    // Provide access to signal types for clients
    using received_signal
        = boost::signals2::signal<void (byte_array const&, socket_endpoint const&)>;
    using ready_transmit_signal = boost::signals2::signal<void ()>;

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

} // comm namespace
} // uia namespace
