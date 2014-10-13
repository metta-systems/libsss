#pragma once

#include <memory>
#include "comm/socket_endpoint.h"
#include "sss/peer_identity.h"
// #include "krypto/krypto.h"

namespace sss {

class host;
class channel;

namespace negotiation {

/**
 * Key initiator maintains host state with respect to initiated key exchanges.
 * One initiator keeps state about key exchange with one peer.
 *
 * XXX make key_initiator an abstract base class like key_responder,
 * calling a create_channel() method when it needs to set up a channel
 * rather than requiring the channel to be passed in at the outset.
 */
class key_initiator : public std::enable_shared_from_this<key_initiator>
{
    friend class key_responder; // still some coupling between the two classes...

    std::shared_ptr<host> host_;
    channel*              channel_{nullptr}; ///< Channel for which we initiated key exchange.
    uia::comm::socket_endpoint  target_;     ///< Remote endpoint we're trying to contact.
    uia::peer_identity    remote_id_; ///< Target's host id (empty if unspecified).
    bool                  early_{true}; ///< This initiator can still be canceled.

    uia::comm::magic_t    magic_{0};
    uint32_t              allowed_methods_{0}; ///< Bitwise set of allowed security methods

    enum methods {
        key_method_aes      = (1 << 0), ///< AES encryption, HMAC-SHA256 auth, JFK DH kex
    };

    /**
     * Current phase of the protocol negotiation.
     * CurveCP-style.
     */
    enum class state {
        idle,
        hello /*init1*/,  // gives server short-term client pk
        cookie /*init2*/, // gives client server's short-term pk
        initiate,         // exchanges long-term sk between server and client - auth phase
        done
    } state_{state::idle};

    sss::async::timer retransmit_timer_;

    // Protocol state set up before sending init1
    byte_array                                 initiator_nonce_;
    byte_array /*boost::array<uint8_t, crypto::hash::size>*/  initiator_hashed_nonce_;
    byte_array                                 initiator_short_term_public_key_;
    byte_array                                 responder_public_key_;

    /**
     * Opaque user info block transmitted in init2 phase.
     */
    byte_array                                 user_data_in_;

    void retransmit(bool fail);

    void done();

protected:
    inline uia::comm::magic_t magic() const { return magic_; }

    virtual std::shared_ptr<channel> create_channel() = 0;

public:
    /// Start key negotiation for a channel that has been bound to a link but not yet activated.
    /// If 'target_peer' is non-empty, only connect to specified host ID.
    /// The key_initiator makes itself the parent of the provided channel,
    /// so that if it is deleted the incomplete channel will be too.
    /// The client must therefore re-parent the channel
    /// after successful key exchange before deleting the key_initiator.
    key_initiator(channel* chn, uia::comm::magic_t magic, peer_identity const& target_peer);
    ~key_initiator();

    /**
     * Actually start init1 phase.
     */
    void exchange_keys();

    /**
     * Cancel all of this KeyInitiator's activities (without actually deleting the object just yet).
     */
    void cancel();

    inline uia::comm::socket_endpoint remote_endpoint() const { return target_; }
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
     * Set/get the opaque information block to pass to the responder.
     * the info block is passed encrypted and authenticated in our init2;
     * the responder can use it to decide whether to accept the connection
     * and to setup any upper-layer protocol parameters for the new channel.
     */
    inline byte_array user_data() const { return user_data_in_; }
    inline void set_user_data(byte_array const& user_data) { user_data_in_ = user_data; }

    void send_hello();    // client->server
    void send_cookie();   // server->client
    void send_initiate(); // client->server

    /**
     * Send completion signal, indicating success when true or failure when false.
     */
    typedef boost::signals2::signal<void (std::shared_ptr<key_initiator>, bool)> completion_signal;
    completion_signal on_completed;
};

} // negotiation namespace
} // sss namespace
