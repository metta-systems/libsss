//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2015, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <set>
#include <boost/signals2/signal.hpp>
#include "sodiumpp/sodiumpp.h"
#include "uia/peer_identity.h"
#include "uia/comm/socket_endpoint.h"
#include "uia/comm/packet_receiver.h"/// fixme
#include "sss/internal/timer.h"
#include "sss/forward_ptrs.h"

namespace sss {
namespace negotiation {

/**
 * This abstract base class manages the responder side of the key exchange.
 * It uses packet_receiver interface as base to receive negotiation protocol
 * control packets and respond to incoming key exchange requests.
 *
 * It forwards received requests to a corresponding key initiator in the host state
 * (via host_->get_initiator()).
 *
 * The implemented subclass of this abstract base is @c stream_responder.
 */
class kex_responder : public uia::comm::packet_receiver
{
    host_ptr host_;

    // Temp state
    sodiumpp::secret_key long_term_key;
    sodiumpp::secret_key short_term_key;
    sodiumpp::secret_key minute_key;
    std::set<std::string> cookie_cache;
    struct client
    {
        std::string short_term_key;
    } client;
    std::string fixmeNeedToRebuildSessionPk;

public:
    /**
     * Create a key exchange responder and set it to listen on a particular socket.
     */
    kex_responder(host_ptr host);

    virtual host_ptr get_host() { return host_; }

    /**
     * Socket calls this with key exchange messages intended for us.
     * @param msg Data packet.
     * @param src Origin endpoint.
     */
    void receive(boost::asio::const_buffer msg, uia::comm::socket_endpoint const& src) override;

    /**
     * Send a probe chunk to some network address,
     * presumably a client we've discovered somehow is trying to reach us,
     * in order to punch a hole in any NATs we may be behind
     * and prod the client into (re-)sending us its "hello" immediately.
     */
    void send_probe(uia::comm::endpoint const& dest);

protected:
    /**
     * kex_responder calls this to check whether to accept a connection,
     * before actually bothering to verify the initiator's identity.
     * The default implementation always returns true.
     */
    virtual bool is_initiator_acceptable(uia::comm::socket_endpoint const& initiator_ep,
                                         uia::peer_identity const& initiator_eid,
                                         byte_array const& user_data);

    /**
     * kex_responder calls this to create a channel requested by a client.
     *
     * The 'user_data_in' contains the information block passed by the client,
     * and the 'user_data_out' block will be passed back to the client.
     * This method can return nullptr to reject the incoming connection.
     */
    virtual channel_uptr create_channel(uia::comm::socket_endpoint const& initiator_ep,
                                        byte_array const& initiator_eid,
                                        byte_array const& user_data_in,
                                        byte_array& user_data_out) = 0;

private:
    // Handlers for incoming kex packets
    void got_probe(uia::comm::socket_endpoint const& src);
    void got_hello(boost::asio::const_buffer msg, uia::comm::socket_endpoint const& src);
    void got_initiate(boost::asio::const_buffer msg, uia::comm::socket_endpoint const& src);
    void send_cookie(std::string clientKey, uia::comm::socket_endpoint const& src);
};

} // negotiation namespace
} // sss namespace
