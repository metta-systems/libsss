//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2015, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "sss/negotiation/kex_responder.h"
#include "arsenal/byte_array_wrap.h"
#include "arsenal/make_unique.h"
#include "arsenal/algorithm.h"
#include "arsenal/flurry.h"
#include "sss/host.h"
#include "sss/channels/channel.h"
#include "sss/framing/packet_format.h"
#include "sss/framing/framing_types.h"

using namespace std;
using namespace sodiumpp;

//=================================================================================================
// Supplemental functions.
//=================================================================================================

namespace {

void
warning(string message)
{
    logger::warning() << "Key exchange - " << message;
}

} // anonymous namespace

namespace sss {
namespace negotiation {

//=================================================================================================
// kex_responder
//=================================================================================================

kex_responder::kex_responder(host_ptr host)
    : packet_receiver(host.get())
    , host_(host)
{
}

bool
kex_responder::is_initiator_acceptable(uia::comm::socket_endpoint const& initiator_ep,
                                       uia::peer_identity const& initiator_eid,
                                       byte_array const& user_data)
{
    return true;
}

void
kex_responder::receive(boost::asio::const_buffer msg, uia::comm::socket_endpoint const& src)
{
    logger::debug() << "kex_responder::receive " << dec << boost::asio::buffer_size(msg)
                    << " bytes from " << src;

    // Find and process the first recognized primary chunk.
    // for (auto chunk : m.chunks) {
    //     switch (chunk.type) {
    //         case key_chunk_type::dh_init1: return got_dh_init1(*chunk.dh_init1, src);
    //         case key_chunk_type::dh_response1:
    //             return got_dh_response1(*chunk.dh_response1, src); // key_initiator method?
    //         case key_chunk_type::dh_init2: return got_dh_init2(*chunk.dh_init2, src);
    //         case key_chunk_type::dh_response2:
    //             return got_dh_response2(*chunk.dh_response2, src); // key_initiator method?
    //         default:
    //             logger::warning() << "Unknown negotiation chunk type " << uint32_t(chunk.type);
    //             break;
    //     }
    // }

    // If there were no recognized primary chunks, it might be just
    // a responder's ping packet for hole punching.
    return got_probe(src);
}

void
kex_responder::got_hello(boost::asio::const_buffer msg, uia::comm::socket_endpoint const& src)
{
    sss::channels::hello_packet_header hello;
    fusionary::read(hello, msg);

    string clientKey = as_string(hello.initiator_shortterm_public_key);
    string nonce     = HELLO_NONCE_PREFIX + as_string(hello.nonce);

    unboxer<recv_nonce> unseal(clientKey, long_term_key, nonce);
    string open = unseal.unbox(as_string(hello.box));

    // Open box contains client's long-term public key which we should check against a blacklist

    // Send cookie packet if we're willing to accept connection.
    // We never resend the cookie (spec 3.1.1), initiator will repeat hello if packets get lost.
    // return send_cookie(clientKey);
}

string
kex_responder::send_cookie(string clientKey)
{
    sss::channels::cookie_packet_header packet;
    sss::channels::responder_cookie cookie;
    secret_key sessionKey; // Generate short-term server key

    fixmeNeedToRebuildSessionPk = sessionKey.pk.get();

    // minute-key secretbox nonce
    random_nonce<8> minuteKeyNonce(MINUTEKEY_NONCE_PREFIX);
    // Client short-term public key + Server short-term secret key
    cookie.box = as_array<80>(
        crypto_secretbox(clientKey + sessionKey.get(), minuteKeyNonce.get(), minute_key.get()));

    // Compressed cookie nonce
    cookie.nonce = as_array<16>(minuteKeyNonce.sequential());

    boxer<random_nonce<8>> seal(clientKey, long_term_key, COOKIE_NONCE_PREFIX);

    // Server short-term public key + cookie
    // Box the cookies
    string box = seal.box(sessionKey.pk.get() + as_string(cookie));
    assert(box.size() == 96 + 32 + 16);

    packet.nonce = as_array<16>(seal.nonce_sequential());
    packet.box   = as_array<144>(box);

    // return make_packet(packet);
    return "";
}

void
kex_responder::got_initiate(boost::asio::const_buffer buf, uia::comm::socket_endpoint const& src)
{
    sss::channels::initiate_packet_header init;
    buf = fusionary::read(init, buf);

    // Try to open the cookie
    string nonce = MINUTEKEY_NONCE_PREFIX + as_string(init.responder_cookie.nonce);

    string cookie =
        crypto_secretbox_open(as_string(init.responder_cookie.box), nonce, minute_key.get());

    // Check that cookie and client match
    // if (as_string(init.initiator_shortterm_public_key) != string(subrange(cookie, 0, 32)))
    // return warning("cookie and client mismatch");

    // Extract server short-term secret key
    // short_term_key = secret_key(public_key(""), subrange(cookie, 32, 32));

    // Open the Initiate box using both short-term keys
    string initiateNonce = INITIATE_NONCE_PREFIX + as_string(init.nonce);

    unboxer<recv_nonce> unseal(
        as_string(init.initiator_shortterm_public_key), short_term_key, initiateNonce);
    string msg = unseal.unbox(as_string(init.box));

    // Extract client long-term public key and check the vouch subpacket.
    string clientLongTermKey; // = subrange(msg, 0, 32);

    string vouchNonce = VOUCH_NONCE_PREFIX; // + string(subrange(msg, 32, 16));

    unboxer<recv_nonce> vouchUnseal(clientLongTermKey, long_term_key, vouchNonce);
    string vouch; //= vouchUnseal.unbox(subrange(msg, 48, 48));

    if (vouch != as_string(init.initiator_shortterm_public_key))
        return warning("vouch subpacket invalid");

    client.short_term_key = vouch;

    // All is good, what's in the payload?
    // @todo Pass payload to the framing layer.
    // This means here we create channel, bind it and start parsing payload data
    // - investigate what this means lifetime-wise.

    // string payload = subrange(msg, 96);
    // hexdump(payload);
}

void
kex_responder::got_probe(uia::comm::socket_endpoint const& src)
{
    // Trigger a retransmission of the dh_init1 packet
    // for each outstanding initiation attempt to the given target.
    // logger::debug() << "Got probe from " << src;

    // @todo This ruins the init/response chain for the DH exchange
    // Peers are left in a perpetual loop of reinstating almost always broken peer channel.
    // To fix this, we might not send R0 packets from the peer being contacted if it detects that
    // the same address is already attempting to establish a session.
    // This is not entirely robust though.
    // The other thing might be replay protection, refuse continuing the contact after dh_init1 if
    // there's a duplicate request coming in (that's how it should work I believe).
    // dh.cpp has r2_cache_ of r2 replay protection data.

    // auto pairs = get_host()->get_initiators(src);
    // while (pairs.first != pairs.second)
    // {
    //     auto initiator = (*pairs.first).second;
    //     ++pairs.first;
    //     if (!initiator or initiator->state_ != key_initiator::state::init1)
    //         continue;

    //     initiator->send_dh_init1();
    // }
}

void
kex_responder::send_probe(uia::comm::endpoint const& dest)
{
    logger::debug() << "Send probe0 to " << dest;
    // for (auto s : get_host()->active_sockets()) {
    //     uia::comm::socket_endpoint ep(s, dest);
    //     send_r0(magic(), ep);
    // }
}

} // negotiation namespace

//=================================================================================================
// kex_host_state
//=================================================================================================

negotiation::kex_initiator_ptr
kex_host_state::get_initiator(byte_array nonce)
{
    auto it = initiators_.find(nonce);
    if (it == initiators_.end()) {
        return nullptr;
    }
    return it->second;
}

pair<kex_host_state::ep_iterator, kex_host_state::ep_iterator>
kex_host_state::get_initiators(uia::comm::endpoint const& ep)
{
    return ep_initiators_.equal_range(ep);
}

void
kex_host_state::register_initiator(byte_array const& nonce,
                                   uia::comm::endpoint const& ep,
                                   negotiation::kex_initiator_ptr ki)
{
    initiators_.insert(make_pair(nonce, ki));
    ep_initiators_.insert(make_pair(ep, ki));
}

void
kex_host_state::unregister_initiator(byte_array const& nonce, uia::comm::endpoint const& ep)
{
    initiators_.erase(nonce);
    ep_initiators_.erase(ep);
}

} // sss namespace
