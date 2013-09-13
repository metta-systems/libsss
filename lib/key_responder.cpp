//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "negotiation/key_responder.h"
#include "negotiation/key_message.h"
#include "crypto.h"
#include "host.h"
#include "byte_array_wrap.h"
#include "flurry.h"

using namespace std;
using namespace ssu;

//===========================================================================================================
// Supplemental functions.
//===========================================================================================================

namespace {

/**
 * Calculate SHA-256 hash of the key response message.
 */
byte_array
calc_signature_hash(ssu::negotiation::dh_group_type group,
    int keylen,
    const byte_array& initiator_hashed_nonce,
    const byte_array& responder_nonce,
    const byte_array& initiator_dh_public_key,
    const byte_array& responder_dh_public_key,
    const byte_array& peer_eid)
{
    byte_array data;
    {
        byte_array_owrap<flurry::oarchive> write(data);
        // Key parameter signing block for init2 and response2 messages.
        write.archive() << group // DH group for public keys
           << keylen // AES key length: 16, 24, or 32
           << initiator_hashed_nonce
           << responder_nonce
           << initiator_dh_public_key
           << responder_dh_public_key
           << peer_eid;
    }

    // make this into a wrapper that calculates given hash type over a byte_array...
    crypto::hash md;
    crypto::hash::value sha256hash;
    md.update(data.as_vector());
    md.finalize(sha256hash);

    return sha256hash;
}

} // anonymous namespace

//===========================================================================================================
// key_responder
//===========================================================================================================

namespace ssu {
namespace negotiation {

key_responder::key_responder(shared_ptr<host> host, magic_t magic)
    : link_receiver(*host, magic)
    , host_(host)
{}

bool key_responder::is_initiator_acceptable(link_endpoint const& initiator_ep,
            byte_array/*peer_id?*/ const& initiator_eid, byte_array const& user_data)
{
    return true;
}

void key_responder::receive(const byte_array& msg, const link_endpoint& src)
{
    byte_array_iwrap<flurry::iarchive> read(msg);
	key_message m;
	read.archive() >> m;
    // XXX here may be some decoding error - at the moment handled in link::receive()

    assert(m.magic == stream_protocol::magic_id);

    for (auto chunk : m.chunks)
    {
        switch (chunk.type)
        {
            case ssu::negotiation::key_chunk_type::dh_init1:
                return got_dh_init1(*chunk.dh_init1, src);
            case ssu::negotiation::key_chunk_type::dh_init2:
                return got_dh_init2(*chunk.dh_init2, src);
            case ssu::negotiation::key_chunk_type::dh_response1:
                return got_dh_response1(*chunk.dh_response1, src);//key_initiator method?
            default:
                logger::warning() << "Unknown negotiation chunk type " << uint32_t(chunk.type);
                break;
        }
    }
};

static void warning(string message)
{
    logger::warning() << "key_responder: " << message;
}

/**
 * Send complete prepared key_message.
 */
static void send(key_message& m, const link_endpoint& target)
{
    byte_array msg;
    {
        byte_array_owrap<flurry::oarchive> write(msg);
        write.archive() << m;
    }
    target.send(msg);
}

static void send(magic_t magic, dh_init1_chunk& r, const link_endpoint& to)
{
    key_message m;
    key_chunk chunk;

    chunk.type = key_chunk_type::dh_init1;
    chunk.dh_init1 = r;

    m.magic = magic;
    m.chunks.push_back(chunk);

    send(m, to);
}

static void send(magic_t magic, dh_init2_chunk& r, const link_endpoint& to)
{
    key_message m;
    key_chunk chunk;

    chunk.type = key_chunk_type::dh_init2;
    chunk.dh_init2 = r;

    m.magic = magic;
    m.chunks.push_back(chunk);

    send(m, to);
}

static void send(magic_t magic, dh_response1_chunk& r, const link_endpoint& to)
{
    key_message m;
    key_chunk chunk;

    chunk.type = key_chunk_type::dh_response1;
    chunk.dh_response1 = r;

    m.magic = magic;
    m.chunks.push_back(chunk);

    send(m, to);
}

void key_responder::got_dh_init1(const dh_init1_chunk& data, const link_endpoint& src)
{
    logger::debug() << "Got dh_init1";

    if (data.key_min_length != 128/8 and data.key_min_length != 192/8 and data.key_min_length != 256/8)
        return warning("invalid minimum AES key length");

    shared_ptr<dh_hostkey_t> hostkey(host_->get_dh_key(data.group)); // get or generate a host key
    if (!hostkey)
        return warning("unrecognized DH key group");
    // if (i1.dhi.size() > DH_size(hk->dh))
        // return;     // Public key too large

    // Generate an unpredictable responder's nonce
    assert(crypto::prng_ok());
    boost::array<uint8_t, crypto::hash::size> responder_nonce;
    crypto::fill_random(responder_nonce);

    // Compute the hash challenge
    byte_array challenge_cookie =
        calc_dh_cookie(hostkey, responder_nonce, data.initiator_hashed_nonce, src);

    // Build and send the response
    dh_response1_chunk response;
    response.group = data.group;
    response.key_min_length = data.key_min_length;
    response.initiator_hashed_nonce = data.initiator_hashed_nonce;
    response.responder_nonce = responder_nonce;
    response.responder_dh_public_key = hostkey->public_key_;
    response.responder_challenge_cookie = challenge_cookie;
    // Don't offer responder's identity (eid, public key and signature) for now.
    send(magic(), response, src);
}

/**
 * We got init2, this means the init1/response1 phase might have been done.
 * Find the key_initiator for this exchange and continue.
 */
void key_responder::got_dh_init2(const dh_init2_chunk& data, const link_endpoint& src)
{
    logger::debug() << "Got dh_init2";
    ssu::negotiation::dh_group_type group = ssu::negotiation::dh_group_type::dh_group_1024;
    int keylen = 16;
    byte_array initiator_hashed_nonce;
    byte_array responder_nonce;
    byte_array initiator_dh_public_key;
    byte_array responder_dh_public_key;
    byte_array peer_eid;
    byte_array hash = calc_signature_hash(group, keylen, initiator_hashed_nonce, responder_nonce, initiator_dh_public_key, responder_dh_public_key, peer_eid);
}

/**
 * We got a response, this means we might've sent a request first, find the corresponding initiator.
 */
void key_responder::got_dh_response1(const dh_response1_chunk& data, const link_endpoint& src)
{
    shared_ptr<key_initiator> initiator = host_->get_initiator(data.initiator_hashed_nonce);
    if (!initiator or initiator->group() != data.group)
        return warning("Got dh_response1 for unknown dh_init1");
    if (initiator->is_done())
        return warning("Got duplicate dh_response1 for completed initiator");

    logger::debug() << "Got dh_response1";

    initiator->send_dh_init2();
}

/**
 * Compute HMAC challenge cookie for DH.
 */
byte_array
key_responder::calc_dh_cookie(shared_ptr<ssu::negotiation::dh_hostkey_t> hostkey,
    const byte_array& responder_nonce,
    const byte_array& initiator_hashed_nonce,
    const ssu::link_endpoint& src)
{
    byte_array data;
    {
        // Put together the data to hash
        auto lval_addr = src.address().to_v4().to_bytes();

        byte_array_owrap<flurry::oarchive> write(data);
        write.archive()
            << hostkey->public_key_
            << responder_nonce
            << initiator_hashed_nonce
            << lval_addr
            << src.port();
    }

    // Compute the keyed hash
    assert(hostkey->hmac_secret_key_.size() == crypto::HMACKEYLEN);

    crypto::hash kmd(hostkey->hmac_secret_key_.as_vector());
    crypto::hash::value mac;
    assert(mac.size() == crypto::HMACLEN);//mmmhm, expected HMACLEN is 16 but we generate 32 bytes HMACs... incompat?
    kmd.update(data.as_vector());
    kmd.finalize(mac);

    return mac;
}

//===========================================================================================================
// key_initiator
//===========================================================================================================

key_initiator::key_initiator(shared_ptr<host> host,
                             channel* channel,
                             link_endpoint const& target,
                             magic_t magic,
                             peer_id const& target_peer)
    : host_(host)
    , channel_(channel)
    , target_(target)
    , magic_(magic)
    , remote_id_(target_peer)
    , retransmit_timer_(host_.get())
    , key_min_length_(128/8)
{
    allowed_methods_ = key_method_aes;

    crypto::fill_random(initiator_nonce_.as_vector());
    crypto::hash kmd(initiator_nonce_.as_vector());
    kmd.finalize(initiator_hashed_nonce_);
}

key_initiator::~key_initiator()
{}

void key_initiator::exchange_keys()
{
    logger::debug() << "Initiating key exchange connection to " << target_ << " peer id " << remote_id_;

    host_->register_dh_initiator(initiator_hashed_nonce_, target_, shared_from_this());

    retransmit_timer_.on_timeout.connect(boost::bind(&key_initiator::retransmit, this, _1));

    send_dh_init1();

    retransmit_timer_.start(async::timer::retry_min);
}

void key_initiator::retransmit(bool fail)
{
    logger::debug() << "Time to retransmit the key exchange packet.";
    if (fail)
    {
        logger::debug() << "Key exchange failed";
        state_ = state::done;
        retransmit_timer_.stop();
        return on_completed(false);
    }

    if (state_ == state::init1) {
        send_dh_init1();
    }
    else if (state_ == state::init2) {
        send_dh_init2();
    }
    retransmit_timer_.restart();
}

void key_initiator::send_dh_init1()
{
    logger::debug() << "Send dh_init1 to " << target_;
    state_ = state::init1;

    // Clear previous initiator state in case it was after init1, we're restarting the init.
    responder_nonce_.clear();
    responder_public_key_.clear();
    responder_challenge_cookie_.clear();
    shared_master_secret_.clear();

    // Construct dh_init1 frame from the current state.
    shared_ptr<dh_hostkey_t> hostkey = host_->get_dh_key(dh_group_); // get or generate a host key
    initiator_public_key_ = hostkey->public_key_;

    dh_init1_chunk init;
    init.group = dh_group_;
    init.key_min_length = key_min_length_;//?
    init.initiator_hashed_nonce = initiator_hashed_nonce_;
    init.initiator_dh_public_key = initiator_public_key_;
    init.responder_eid.clear();

    send(magic(), init, target_);
}

void key_initiator::send_dh_init2()
{
    logger::debug() << "Send dh_init2 to " << target_;
    state_ = state::init2;

    dh_init2_chunk init;
    send(magic(), init, target_);
}

} // namespace negotiation

//===========================================================================================================
// key_host_state
//===========================================================================================================

shared_ptr<ssu::negotiation::key_initiator>
key_host_state::get_initiator(byte_array nonce)
{
    auto it = dh_initiators_.find(nonce);
    if (it == dh_initiators_.end()) {
        return 0;
    }
    return it->second;
}

void
key_host_state::register_dh_initiator(byte_array const& nonce,
                                   endpoint const& ep,
                                   shared_ptr<ssu::negotiation::key_initiator> ki)
{
    dh_initiators_.insert(make_pair(nonce, ki));
    ep_initiators_.insert(make_pair(ep, ki));
}

} // namespace ssu
