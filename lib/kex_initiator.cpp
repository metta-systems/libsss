//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "sss/negotiation/kex_initiator.h"
#include "sss/negotiation/kex_message.h"
#include "arsenal/byte_array_wrap.h"
#include "arsenal/make_unique.h"
#include "arsenal/algorithm.h"
#include "arsenal/flurry.h"
#include "krypto/sha256_hash.h"
#include "krypto/aes_256_cbc.h"
// #include "sss/aes_armor.h"
#include "sss/host.h"
#include "sss/channels/channel.h"

using namespace std;

namespace sss {
namespace negotiation {

//=================================================================================================
// kex_initiator
//=================================================================================================

kex_initiator::kex_initiator(uia::comm::endpoint target,
                             peer_identity const& target_peer)
    : host_(channel->get_host())
    , target_(target)
    , remote_id_(target_peer)
    , magic_(magic)
    , retransmit_timer_(channel->get_host().get())
{
    logger::debug() << "Creating kex_initiator " << this;

    assert(target_ != uia::comm::endpoint());
    assert(channel->is_bound());
    assert(!channel->is_active());
}

kex_initiator::~kex_initiator()
{
    logger::debug() << "Destroying kex_initiator " << this;
    cancel();
}

void
kex_initiator::exchange_keys()
{
    logger::debug() << "Initiating key exchange connection to " << target_ << " peer id " << remote_id_;

    host_->register_initiator(initiator_hashed_nonce_, target_, shared_from_this());

    retransmit_timer_.on_timeout.connect([this](bool fail) {
        retransmit(fail);
    });

    send_hello();

    retransmit_timer_.start();
}

void
kex_initiator::retransmit(bool fail)
{
    if (fail)
    {
        logger::debug() << "Key exchange failed";
        state_ = state::done;
        retransmit_timer_.stop();
        return on_completed(shared_from_this(), false);
    }

    logger::debug() << "Time to retransmit the key exchange packet.";

    // If we're gonna resend the init packet, make sure we are registered as a receiver for
    // response packets.
    host_->register_initiator(initiator_hashed_nonce_, target_, shared_from_this());

    if (state_ == state::hello) {
        send_hello();
    }
    else if (state_ == state::initiate) {
        send_initiate();
    }
    retransmit_timer_.restart();
}

void
kex_initiator::done()
{
    bool send_signal = (state_ != state::done);
    logger::debug() << "Key exchange completed with " << target_ << (send_signal ? " (signaling upper layer)" : "");
    state_ = state::done;
    cancel();
    if (send_signal) {
        on_completed(shared_from_this(), true);
    }
}

void
kex_initiator::cancel()
{
    logger::debug() << "Stop initiating to " << target_;
    retransmit_timer_.stop();
    host_->unregister_initiator(initiator_hashed_nonce_, target_);
}

void
kex_initiator::send_hello()
{
    logger::debug() << "Send hello to " << target_;
    state_ = state::hello;

    // Clear previous initiator state in case it was after hello, we're restarting the init.
    responder_nonce_.clear();
    responder_public_key_.clear();
    responder_challenge_cookie_.clear();
    shared_master_secret_.clear();

    // Construct kex_hello from the current state.
    shared_ptr<dh_hostkey_t> hostkey = host_->get_dh_key(dh_group_); // get or generate a host key
    initiator_public_key_ = hostkey->public_key_;

    kex_hello_chunk init;
    init.initiator_hashed_nonce = initiator_hashed_nonce_;
    init.initiator_dh_public_key = initiator_public_key_;

    send(magic(), init, target_);
}

void
kex_initiator::send_cookie()
{
    logger::debug() << "Send dh_init2 to " << target_;
    state_ = state::init2;

    // Once our peer receives dh_init2 he will create channel state.
    early_ = false; // do this in initiate instead

    kex_cookie_chunk init;
    init.initiator_nonce = initiator_nonce_;
    init.responder_nonce = responder_nonce_;
    init.initiator_dh_public_key = initiator_public_key_;
    init.responder_dh_public_key = responder_public_key_;
    init.responder_challenge_cookie = responder_challenge_cookie_;
    init.initiator_info = encrypted_identity_info_;

    send(magic(), init, target_);
}

} // negotiation namespace
} // sss namespace
