//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2015, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <memory>
#include <boost/signals2/signal.hpp>
#include "sodiumpp/sodiumpp.h"
#include "uia/peer_identity.h"
#include "uia/comm/socket_endpoint.h"
#include "sss/channels/channel.h"
#include "sss/internal/timer.h"
#include "sss/forward_ptrs.h"

namespace sss {
namespace negotiation {

/**
 * Key exchange initiator maintains host state with respect to initiated key exchanges.
 * One initiator keeps state about key exchange with one peer.
 *
 * XXX make key_initiator an abstract base class like key_responder,
 * calling a create_channel() method when it needs to set up a channel
 * rather than requiring the channel to be passed in at the outset.
 *
 * The implemented class of this abstract base is @c stream_initiator.
 */

// @todo Single initiator may set up channel to single peer_identity on multiple endpoints?

// kex_responder side:
// Hello and Cookie processing requires no state.
// Only when Initiate is received and passes validation we create channel
// and start it.
//
// kex_initiator side:
// 1) attempt to send Hello packets periodically until we get a Cookie or give up
// 2) when Cookie is received, allocate our state and attempt to send Initiate with
// some data.
// 3) If after retrying Initiate we don't get response for 30 seconds, send another Hello.
//
class kex_initiator : public std::enable_shared_from_this<kex_initiator>
{
    host_ptr host_;
    uia::comm::socket_endpoint target_; ///< Remote endpoint we're trying to contact.
    uia::peer_identity remote_id_;      ///< Target's host id (empty if unspecified).
    bool early_{true};                  ///< This initiator can still be canceled.

    /**
     * Current phase of the protocol negotiation.
     */
    enum class state
    {
        idle,
        hello,    // gives server client's short-term public key
        initiate, // auth phase
        done
    } state_{state::idle};

    sss::async::timer retransmit_timer_;

    void retransmit(bool fail);

    void done();

    // Key exchange state

    // We know server long term public key on start - this is remote_id_
    // We need to remember short-term server public key
    sodiumpp::secret_key short_term_secret_key; // out short-term key (generated)
    std::string server_short_term_public_key; // remote_peer.short_term key

    std::string minute_cookie_; // one-minute cookie received after hello packet response

protected:
    virtual channel_ptr create_channel() {return nullptr;}//= 0;

public:
    /// Start key negotiation with remote peer. If successful, this negotiation will yield a
    /// new channel via `create_channel()` call.
    kex_initiator(host_ptr host, uia::peer_identity const& target_peer,
        uia::comm::socket_endpoint target);
    ~kex_initiator();

    /**
     * Actually start hello phase.
     */
    void exchange_keys();

    /**
     * Cancel all of this kex_initiator's activities
     * (without actually deleting the object just yet).
     */
    void cancel();

    inline uia::comm::socket_endpoint remote_endpoint() const { return target_; }
    inline bool is_done() const { return state_ == state::done; }

    /**
     * Key exchange protocol from the initiator standpoint.
     */
    void send_hello();
    void got_cookie(boost::asio::const_buffer buf, uia::comm::socket_endpoint const& src);
    void send_initiate(std::string cookie, std::string payload);

    /**
     * Send completion signal, giving created channel on success or nullptr on failure.
     */
    using completion_signal = boost::signals2::signal<void(kex_initiator_ptr, channel_ptr)>;
    completion_signal on_completed;
};

} // negotiation namespace
} // sss namespace
