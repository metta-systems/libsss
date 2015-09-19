//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "sss/negotiation/kex_initiator.h"
#include "arsenal/byte_array_wrap.h"
#include "arsenal/make_unique.h"
#include "arsenal/algorithm.h"
#include "arsenal/flurry.h"
#include "sss/host.h"
#include "sss/channels/channel.h"

using namespace std;
using namespace sodiumpp;

namespace sss {
namespace negotiation {

//=================================================================================================
// kex_initiator
//=================================================================================================

kex_initiator::kex_initiator(host_ptr host, uia::peer_identity const& target_peer)
    : host_(host)
    , remote_id_(target_peer)
    , retransmit_timer_(host.get())
{
    logger::debug() << "Creating kex_initiator " << this;

    assert(target_ != uia::comm::endpoint());
    // assert(channel->is_bound());
    // assert(!channel->is_active());
}

kex_initiator::~kex_initiator()
{
    logger::debug() << "Destroying kex_initiator " << this;
    cancel();
}

void
kex_initiator::exchange_keys()
{
    logger::debug() << "Initiating key exchange connection to " << target_ << " peer id "
                    << remote_id_;

    host_->register_initiator(byte_array(), target_, shared_from_this());

    retransmit_timer_.on_timeout.connect([this](bool fail) { retransmit(fail); });

    send_hello();

    retransmit_timer_.start();
}

void
kex_initiator::retransmit(bool fail)
{
    if (fail) {
        logger::debug() << "Key exchange failed";
        state_ = state::done;
        retransmit_timer_.stop();
        return on_completed(shared_from_this(), nullptr); //@todo
    }

    logger::debug() << "Time to retransmit the key exchange packet.";

    // If we're gonna resend the init packet, make sure we are registered as a receiver for
    // response packets.
    host_->register_initiator(byte_array(), target_, shared_from_this());

    if (state_ == state::hello) {
        // send_hello();
    } else if (state_ == state::initiate) {
        // send_initiate();
    }
    retransmit_timer_.restart();
}

void
kex_initiator::done()
{
    bool send_signal = (state_ != state::done);
    logger::debug() << "Key exchange completed with " << target_
                    << (send_signal ? " (signaling upper layer)" : "");
    state_ = state::done;
    cancel();
    if (send_signal) {
        on_completed(shared_from_this(), nullptr); //@todo chan
    }
}

void
kex_initiator::cancel()
{
    logger::debug() << "Stop initiating to " << target_;
    retransmit_timer_.stop();
    host_->unregister_initiator(byte_array(), target_);
}

void
kex_initiator::send_hello()
{
    logger::debug() << "Send hello to " << target_;
    state_ = state::hello;

    boxer<nonce64> seal(server.long_term_key, short_term_key, helloNoncePrefix);

    sss::channels::hello_packet_header pkt;
    pkt.initiator_shortterm_public_key = as_array<32>(short_term_key.pk.get());
    pkt.box                            = as_array<80>(seal.box(long_term_key.pk.get() + string(32, '\0')));
    pkt.nonce                          = as_array<8>(seal.nonce_sequential());

    // return make_packet(pkt);
}

void
kex_initiator::got_cookie()
{
    sss::channels::cookie_packet_header cookie;
    asio::const_buffer buf(pkt.data(), pkt.size());
    tie(cookie, ignore) = fusionary::read<sss::channels::cookie_packet_header>(buf);

    // open cookie box
    string nonce = cookieNoncePrefix + as_string(cookie.nonce);

    unboxer<recv_nonce> unseal(server.long_term_key, short_term_key, nonce);
    string open = unseal.unbox(as_string(cookie.box));

    // server.short_term_key = subrange(open, 0, 32);
    // string cookie_buf     = subrange(open, 32, 96);

    // @todo Must get payload from client
    // return send_initiate(cookie_buf, "Hello, world!");
}

void
kex_initiator::send_initiate()
{
    // Create vouch subpacket
    boxer<random_nonce<8>> vouchSeal(server.long_term_key, long_term_key, vouchNoncePrefix);
    string vouch = vouchSeal.box(short_term_key.pk.get());
    assert(vouch.size() == 48);

    // Assemble initiate packet
    sss::channels::initiate_packet_header pkt;
    pkt.initiator_shortterm_public_key = as_array<32>(short_term_key.pk.get());
    // pkt.responder_cookie.nonce         = as_array<16>(subrange(cookie, 0, 16));
    // pkt.responder_cookie.box           = as_array<80>(subrange(cookie, 16));

    boxer<nonce64> seal(server.short_term_key, short_term_key, initiateNoncePrefix);
    pkt.box = seal.box(long_term_key.pk.get() + vouchSeal.nonce_sequential() + vouch + payload);
    // @todo Round payload size to next or second next multiple of 16..
    pkt.nonce = as_array<8>(seal.nonce_sequential());

    // return make_packet(pkt);
}

} // negotiation namespace
} // sss namespace
