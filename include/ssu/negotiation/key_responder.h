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
#include "ssu/link.h"
#include "ssu/timer.h"
#include "krypto/krypto.h"
#include "ssu/peer_id.h"
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
class key_responder : public link_receiver
{
public:
    /**
     * Create a key_responder and set it listening on a particular link
     * for control messages with the specified magic protocol identifier.
     * @fixme The new key_responder becomes a child of the link.
     */
    key_responder(std::shared_ptr<host> host, magic_t magic);

    /**
     * Link calls this with control messages intended for us.
     * @param msg [description]
     * @param src [description]
     */
    void receive(const byte_array& msg, const link_endpoint& src) override;

    /**
     * Send an R0 chunk to some network address,
     * presumably a client we've discovered somehow is trying to reach us,
     * in order to punch a hole in any NATs we may be behind
     * and prod the client into (re-)sending us its I1 immediately.
     */
    void send_probe0(endpoint const& dest);

protected:
    /**
     * key_responder calls this to check whether to accept a connection,
     * before actually bothering to verify the initiator's identity.
     * The default implementation always returns true.
     */
    virtual bool is_initiator_acceptable(link_endpoint const& initiator_ep,
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
    virtual channel* create_channel(link_endpoint const& initiator_ep,
            byte_array const& initiator_eid,
            byte_array const& user_data_in, byte_array& user_data_out) = 0;

private:
    void got_probe0(const link_endpoint& src);
    void got_dh_init1(const dh_init1_chunk& data, const link_endpoint& src);
    void got_dh_response1(const dh_response1_chunk& data, const link_endpoint& src);
    void got_dh_init2(const dh_init2_chunk& data, const link_endpoint& src);
    void got_dh_response2(const dh_response2_chunk& data, const link_endpoint& src);

    byte_array
    calc_dh_cookie(std::shared_ptr<ssu::negotiation::dh_hostkey_t> hostkey,
        const byte_array& responder_nonce,
        const byte_array& initiator_hashed_nonce,
        const ssu::link_endpoint& src);
};

/**
 * Key initiator maintains host state with respect to initiated key exchanges.
 * One initiator keeps state about key exchange with one peer.
 *
 * XXX we should really have a separate Idle state,
 * so that clients can hookup signals before starting key exchange.
 * XXX make key_initiator an abstract base class like key_responder,
 * calling a create_channel() method when it needs to set up a channel
 * rather than requiring the channel to be passed in at the outset.
 */
class key_initiator : public std::enable_shared_from_this<key_initiator>
{
    friend class key_responder; // still some coupling between the two classes...

    std::shared_ptr<host> host_;
    channel*              channel_{nullptr}; ///< Channel for which we initiated key exchange.
    link_endpoint         target_;    ///< Remote endpoint we're trying to contact.
    peer_id               remote_id_; ///< Target's host id (empty if unspecified).
    bool                  early_{true}; ///< This initiator can still be canceled.

    magic_t               magic_{0};
    uint32_t              allowed_methods_{0}; ///< Bitwise set of allowed security methods

    enum methods {
        key_method_aes      = (1 << 0), ///< AES encryption, HMAC-SHA256 auth, JFK DH kex
    };

    /**
     * Current phase of the protocol negotiation.
     */
    enum class state {
        idle, init1, init2, done
    } state_{state::idle};

    ssu::async::timer retransmit_timer_;

    // AES/SHA256 with DH key agreement

    /// DH group to use
    ssu::negotiation::dh_group_type            dh_group_{dh_group_type::dh_group_3072};
    /// AES key length to use
    int                                        key_min_length_{0};

    // Protocol state set up before sending init1
    byte_array                                 initiator_nonce_;
    boost::array<uint8_t, crypto::hash::size>  initiator_hashed_nonce_;
    byte_array                                 initiator_public_key_;

    // Set after receiving response1
    byte_array                                 responder_nonce_;
    byte_array                                 responder_public_key_;
    byte_array                                 responder_challenge_cookie_;
    byte_array                                 shared_master_secret_;
    /**
     * Encrypted and authenticated identity information.
     */
    byte_array                                 encrypted_identity_info_;

    /**
     * Opaque user info block transmitted in init2 phase.
     */
    byte_array                                 user_data_in_;

    void retransmit(bool fail);

    void done();

protected:
    inline magic_t magic() const { return magic_; }

public:
    /// Start key negotiation for a channel that has been bound to a link but not yet activated.
    /// If 'target_peer' is non-empty, only connect to specified host ID.
    /// The key_initiator makes itself the parent of the provided channel,
    /// so that if it is deleted the incomplete channel will be too.
    /// The client must therefore re-parent the channel
    /// after successful key exchange before deleting the key_initiator.
    key_initiator(channel* chn, magic_t magic, peer_id const& target_peer);
    ~key_initiator();

    /**
     * Actually start init1 phase.
     */
    void exchange_keys();

    inline link_endpoint remote_endpoint() const { return target_; }
    inline ssu::negotiation::dh_group_type group() const { return dh_group_; }
    inline bool is_done() const { return state_ == state::done; }

    /// Returns true if this key_initiator hasn't gotten far enough
    /// so that the remote peer might possibly create permanent state
    /// if we cancel the process at our end now.
    /// We use this if we're trying to initiate a connection to a peer
    /// but that peer contacts us first, giving us a primary channel;
    /// we can then abort our outstanding active initiation attempts
    /// ONLY if they're still in an early enough stage that we know
    /// the responder won't be left with a dangling end of a new channel.
    inline bool is_early() const { return early_; }

    /**
     * Cancel all of this KeyInitiator's activities (without actually deleting the object just yet).
     */
    void cancel();

    /**
     * Set/get the opaque information block to pass to the responder.
     * the info block is passed encrypted and authenticated in our init2;
     * the responder can use it to decide whether to accept the connection
     * and to setup any upper-layer protocol parameters for the new channel.
     */
    inline byte_array user_data() const { return user_data_in_; }
    inline void set_user_data(byte_array const& user_data) { user_data_in_ = user_data; }

    void send_dh_init1();
    void send_dh_response1();
    void send_dh_init2();
    void send_dh_response2();

    /**
     * Send completion signal, indicating success when true or failure when false.
     */
    typedef boost::signals2::signal<void (std::shared_ptr<key_initiator>, bool)> completion_signal;
    completion_signal on_completed;
};

} // namespace negotiation

/**
 * Mixin for host state that manages the key exchange state.
 */
class key_host_state
{
    /**
     * Initiators by nonce.
     */
    std::unordered_map<byte_array, std::shared_ptr<negotiation::key_initiator>> dh_initiators_;
    /**
     * Initiators by endpoint.
     * Used for handling R0 packets during hole-punching.
     */
    std::unordered_multimap<endpoint, std::shared_ptr<negotiation::key_initiator>> ep_initiators_;

public:
    typedef std::unordered_multimap<endpoint, std::shared_ptr<negotiation::key_initiator>>::iterator ep_iterator;

    std::shared_ptr<negotiation::key_initiator> get_initiator(byte_array nonce);
    std::pair<ep_iterator, ep_iterator> get_initiators(endpoint const& ep);

    void register_dh_initiator(byte_array const& nonce, endpoint const& ep,
        std::shared_ptr<ssu::negotiation::key_initiator> ki);
    void unregister_dh_initiator(byte_array const& nonce, endpoint const& ep);
};

} // namespace ssu
