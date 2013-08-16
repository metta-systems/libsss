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

namespace ssu {

/**
 * Base class for socket-based channels,
 * for dispatching received packets based on endpoint and channel number.
 * May be used as an abstract base by overriding the receive() method,
 * or used as a concrete class by connecting to the on_received signal.
 */
class link_channel
{
public:
    /**
     * Start the channel.
     * @param initiate Initiate the key exchange using key_initiator.
     */
    virtual void start(bool initiate);
    /**
     * Stop the channel.
     */
    virtual void stop();

    inline bool is_active()       const { return active_; }
    inline bool is_bound()        const { return link_.lock() != nullptr; }

    /**
     * Set up for communication with specified remote endpoint,
     * allocating and binding a local channel number in the process.
     * @returns 0 if no channels are available for specified endpoint.
     */
    channel_number bind(std::shared_ptr<link> link, const endpoint& remote_ep);
    inline channel_number bind(const link_endpoint& remote_ep) {
        return bind(remote_ep.link(), remote_ep);
    }

    /**
     * Bind to a particular channel number.
     * @returns false if the channel is already in use.
     */
    bool bind(std::shared_ptr<link> link, const endpoint& remote_ep, channel_number chan);

    /**
     * Stop channel and unbind from any currently bound remote endpoint.
     */
    void unbind();

    // Provide access to signal types for clients
    typedef boost::signals2::signal<void (byte_array&, const link_endpoint&)> received_signal;
    typedef boost::signals2::signal<void ()> ready_transmit_signal;

    received_signal on_received;
    // Signalled when channel congestion control may allow new transmission.
    ready_transmit_signal on_ready_transmit;

protected:
    friend class link;

    inline bool send(const byte_array& pkt) const {
        assert(active_);
        if (auto l = link_.lock()) {
            return l->send(remote_ep_, pkt);
        }
        return false;
    }

    virtual void receive(const byte_array& /*msg*/, const link_endpoint& /*src*/) {}

private:
    std::weak_ptr<link> link_;      ///< Link we're currently bound to, if any.
    endpoint            remote_ep_; ///< Endpoint of the remote side.
    bool                active_;    ///< True if we're sending and accepting packets.
};

} // ssu namespace
