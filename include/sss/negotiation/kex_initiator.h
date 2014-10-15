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

// kex_responder side:
// Hello and Cookie processing requires no state.
// Only when Initiate is received and passes validation we create channel
// and start it.
//
// kex_initiator side:
// 1) attempt to send Hello packets periodically until we get a Cookie or give up
// 2) when Cookie is received, allocate our state and attempt to send Initiate with
// some data.
//
class kex_initiator : public std::enable_shared_from_this<kex_initiator>
{
    std::shared_ptr<host> host_;
    uia::comm::socket_endpoint  target_;     ///< Remote endpoint we're trying to contact.
    uia::peer_identity    remote_id_; ///< Target's host id (empty if unspecified).
    bool                  early_{true}; ///< This initiator can still be canceled.

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

    void retransmit(bool fail);

    void done();

protected:
    virtual std::shared_ptr<channel> create_channel() = 0;

public:
    typedef std::shared_ptr<kex_initiator> ptr;

    /// Start key negotiation for a channel that has been bound to a link but not yet activated.
    /// If 'target_peer' is non-empty, only connect to specified host ID.
    kex_initiator(uia::peer_identity const& target_peer);
    ~kex_initiator();

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

    /**
     * Key exchange protocol.
     */
    void send_hello();
    void got_cookie();
    void send_initiate();

    /**
     * Send completion signal, indicating success when true or failure when false.
     */
    typedef boost::signals2::signal<void (kex_initiator::ptr, bool)> completion_signal;
    completion_signal on_completed;
};

} // negotiation namespace
} // sss namespace
