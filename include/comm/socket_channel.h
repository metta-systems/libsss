//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <boost/signals2/signal.hpp>
#include "arsenal/byte_array.h"
// #include "ssu/protocol.h"
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
class socket_channel
{
    socket*        socket_{nullptr};          ///< Socket we're currently bound to, if any.
    endpoint       remote_ep_;                ///< Endpoint of the remote side.
    channel_number local_channel_number_{0};  ///< Channel number of this channel at local node.
    channel_number remote_channel_number_{0}; ///< Channel number of this channel at remote node.
    bool           active_{false};            ///< True if we're sending and accepting packets.

public:
    inline virtual ~socket_channel() {
        unbind();
    }

    /**
     * Start the channel.
     * @param initiate Initiate the key exchange using key_initiator.
     */
    inline virtual void start(bool initiate)
    {
        assert(remote_channel_number_);
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
        return socket_ != nullptr;
    }

    /**
     * Test whether underlying socket is already congestion controlled.
     */
    inline bool is_socket_congestion_controlled() {
        return socket_->is_congestion_controlled(remote_ep_);
    }

    /**
     * Return the remote endpoint we're bound to, if any.
     */
    inline socket_endpoint remote_endpoint() const {
        return uia::comm::socket_endpoint(socket_, remote_ep_);
    }

    /**
     * Set up for communication with specified remote endpoint,
     * allocating and binding a local channel number in the process.
     * @returns 0 if no channels are available for specified endpoint.
     */
    channel_number bind(socket* socket, endpoint const& remote_ep);

    /**
     * Set up for communication with specified remote endpoint,
     * allocating and binding a local channel number in the process.
     * @returns 0 if no channels are available for specified endpoint.
     * @override
     */
    inline channel_number bind(socket_endpoint const& remote_ep) {
        return bind(remote_ep.socket(), remote_ep);
    }

    /**
     * Bind to a particular channel number.
     * @returns false if the channel is already in use and cannot be bound to.
     */
    bool bind(socket* socket, endpoint const& remote_ep, channel_number chan);

    /**
     * Stop channel and unbind from any currently bound remote endpoint.
     */
    void unbind();

    /**
     * Return current local channel number.
     */
    inline channel_number local_channel() const {
        return local_channel_number_;
    }

    /**
     * Return current remote channel number.
     */
    inline channel_number remote_channel() const {
        return remote_channel_number_;
    }

    /**
     * Set the channel number to direct packets to the remote endpoint.
     * This MUST be done before a new channel can be activated.
     */
    inline void set_remote_channel(channel_number ch) {
        remote_channel_number_ = ch;
    }

    inline virtual void receive(byte_array const& msg, socket_endpoint const& src) {
        on_received(msg, src);
    }

    /** @name Signals. */
    /**@{*/
    // Provide access to signal types for clients
    typedef
        boost::signals2::signal<void (byte_array const&, socket_endpoint const&)>
        received_signal;
    typedef boost::signals2::signal<void ()> ready_transmit_signal;

    received_signal on_received;
    /**
     * Signalled when channel congestion control may allow new transmission.
     */
    ready_transmit_signal on_ready_transmit;
    /**@}*/

protected:
    /**
     * When the underlying socket is already congestion-controlled, this function returns
     * the number of packets that channel control says we may transmit now, 0 if none.
     */
    virtual int may_transmit();

    inline bool send(byte_array const& pkt) const
    {
        assert(active_);
        if (auto s = socket_) {
            return s->send(remote_ep_, pkt);
        }
        return false;
    }
};

} // comm namespace
} // uia namespace
