//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <memory>
#include <boost/signals2/signal.hpp>
#include "comm/socket_endpoint.h"
#include "comm/socket_receiver.h"
#include "ssu/timer.h"
#include "ssu/negotiation/key_message.h"

namespace ssu {

class host;
class channel;

namespace negotiation {

class dh_hostkey_t;

/**
 * This abstract base class manages the responder side of the key exchange.
 * It uses link_receiver interface as base to receive negotiation protocol control messages
 * and respond to incoming key exchange requests.
 *
 * It forwards received requests to a corresponding key initiator in the host state
 * (via host_->get_initiator()).
 *
 * The implemented subclass of this abstract base is stream_responder.
 */
class key_responder : public uia::comm::socket_receiver
{
    std::shared_ptr<host> host_;

public:
    /**
     * Create a key_responder and set it listening on a particular link
     * for control messages with the specified magic protocol identifier.
     * @fixme The new key_responder becomes a child of the link.
     */
    key_responder(std::shared_ptr<host> host, uia::comm::magic_t magic);

    virtual std::shared_ptr<host> get_host() { return host_; }

    /**
     * Link calls this with control messages intended for us.
     * @param msg [description]
     * @param src [description]
     */
    void receive(const byte_array& msg, uia::comm::socket_endpoint const& src) override;

    /**
     * Send an R0 chunk to some network address,
     * presumably a client we've discovered somehow is trying to reach us,
     * in order to punch a hole in any NATs we may be behind
     * and prod the client into (re-)sending us its I1 immediately.
     */
    void send_probe0(uia::comm::endpoint const& dest);

protected:
    /**
     * key_responder calls this to check whether to accept a connection,
     * before actually bothering to verify the initiator's identity.
     * The default implementation always returns true.
     */
    virtual bool is_initiator_acceptable(uia::comm::socket_endpoint const& initiator_ep,
            byte_array/*peer_id?*/ const& initiator_eid, byte_array const& user_data);

    /**
     * key_responder calls this to create a channel requested by a client.
     * The returned channel must be bound to the specified source endpoint,
     * but not yet active (started).
     *
     * The 'user_data_in' contains the information block passed by the client,
     * and the 'user_data_out' block will be passed back to the client.
     * This method can return nullptr to reject the incoming connection.
     */
    virtual channel* create_channel(uia::comm::socket_endpoint const& initiator_ep,
            byte_array const& initiator_eid,
            byte_array const& user_data_in, byte_array& user_data_out) = 0;

private:
    void got_probe0(uia::comm::socket_endpoint const& src);
    void got_dh_init1(const dh_init1_chunk& data, uia::comm::socket_endpoint const& src);
    void got_dh_response1(const dh_response1_chunk& data, uia::comm::socket_endpoint const& src);
    void got_dh_init2(const dh_init2_chunk& data, uia::comm::socket_endpoint const& src);
    void got_dh_response2(const dh_response2_chunk& data, uia::comm::socket_endpoint const& src);

    byte_array
    calc_dh_cookie(std::shared_ptr<ssu::negotiation::dh_hostkey_t> hostkey,
        const byte_array& responder_nonce,
        const byte_array& initiator_hashed_nonce,
        const uia::comm::socket_endpoint& src);
};

} // namespace negotiation
} // namespace ssu
