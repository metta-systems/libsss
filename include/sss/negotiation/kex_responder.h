//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <memory>
#include <boost/signals2/signal.hpp>
#include "comm/socket_endpoint.h"
#include "comm/packet_receiver.h"
#include "sss/internal/timer.h"
#include "sss/negotiation/kex_message.h"

namespace sss {

class host;
class channel;

namespace negotiation {

/**
 * This abstract base class manages the responder side of the key exchange.
 * It uses packet_receiver interface as base to receive negotiation protocol
 * control messages and respond to incoming key exchange requests.
 *
 * It forwards received requests to a corresponding key initiator in the host state
 * (via host_->get_initiator()).
 *
 * The implemented subclass of this abstract base is stream_responder.
 */
class kex_responder : public uia::comm::packet_receiver
{
    std::shared_ptr<host> host_;

public:
    /**
     * Create a key exchange responder and set it listening on a particular link.
     * @fixme The new key_responder becomes a child of the link.
     */
    kex_responder(std::shared_ptr<host> host);

    virtual std::shared_ptr<host> get_host() { return host_; }

    /**
     * Link calls this with control messages intended for us.
     * @param msg [description]
     * @param src [description]
     */
    void receive(byte_array const& msg, uia::comm::socket_endpoint const& src) override;

    /**
     * Send an R0 chunk to some network address,
     * presumably a client we've discovered somehow is trying to reach us,
     * in order to punch a hole in any NATs we may be behind
     * and prod the client into (re-)sending us its "hello" immediately.
     */
    void send_probe0(uia::comm::endpoint const& dest);

protected:
    /**
     * kex_responder calls this to check whether to accept a connection,
     * before actually bothering to verify the initiator's identity.
     * The default implementation always returns true.
     */
    virtual bool is_initiator_acceptable(uia::comm::socket_endpoint const& initiator_ep,
            byte_array/*peer_identity?*/ const& initiator_eid, byte_array const& user_data);

    /**
     * kex_responder calls this to create a channel requested by a client.
     * The returned channel must be bound to the specified source endpoint,
     * but not yet active (started).
     *
     * The 'user_data_in' contains the information block passed by the client,
     * and the 'user_data_out' block will be passed back to the client.
     * This method can return nullptr to reject the incoming connection.
     */
    virtual std::unique_ptr<channel> create_channel(uia::comm::socket_endpoint const& initiator_ep,
            byte_array const& initiator_eid,
            byte_array const& user_data_in, byte_array& user_data_out) = 0;

private:
    // Handlers for incoming kex packets
    void got_probe0(uia::comm::socket_endpoint const& src);
    void got_hello(kex_hello_chunk const& data, uia::comm::socket_endpoint const& src);
    void got_cookie(kex_cookie_chunk const& data, uia::comm::socket_endpoint const& src);
    void got_initiate(kex_initiate_chunk const& data, uia::comm::socket_endpoint const& src);
};

} // negotiation namespace
} // sss namespace
