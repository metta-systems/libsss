//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include <memory>
#include "negotiation/key_responder.h"
#include "negotiation/key_message.h"
#include "crypto.h"
#include "crypto/sha256_hash.h"
#include "crypto/aes_256_cbc.h"
#include "crypto/aes_armor.h"
#include "host.h"
#include "byte_array_wrap.h"
#include "flurry.h"
#include "channel.h"

using namespace std;
using namespace ssu;

//=================================================================================================
// Supplemental functions.
//=================================================================================================

namespace {

/**
 * Calculate SHA-256 hash of the key response message.
 */
byte_array
calc_signature_hash(ssu::negotiation::dh_group_type group,
    int keylen,
    byte_array const& initiator_hashed_nonce,
    byte_array const& responder_nonce,
    byte_array const& initiator_dh_public_key,
    byte_array const& responder_dh_public_key,
    byte_array const& peer_eid)
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

    return sha256::hash(data);
}

byte_array
calc_key(byte_array const& master,
    byte_array const& initiator_hashed_nonce,
    byte_array const& responder_nonce,
    char which, int keylen)
{
    byte_array master_hash = sha256::hash(master);
    assert(master_hash.size() == crypto::HMACKEYLEN);

    crypto::hash hmac(master_hash.as_vector());
    hmac.update(initiator_hashed_nonce.as_vector());
    hmac.update(responder_nonce.as_vector());
    hmac.update(byte_array({which}).as_vector());

    crypto::hash::value key;
    hmac.finalize(key);

    byte_array out = key;
    out.resize(keylen);
    return out;
}

void warning(string message)
{
    logger::warning() << "Key exchange - " << message;
}

} // anonymous namespace

namespace ssu {
namespace negotiation {

//=================================================================================================
// send helpers
//=================================================================================================

namespace {

/**
 * Send complete prepared key_message.
 */
byte_array send(key_message& m, link_endpoint const& target)
{
    byte_array msg;
    {
        byte_array_owrap<flurry::oarchive> write(msg);
        write.archive() << m;
    }
    target.send(msg);
    return msg;
}

void send(magic_t magic, dh_init1_chunk& r, link_endpoint const& to)
{
    key_message m;
    key_chunk chunk;

    chunk.type = key_chunk_type::dh_init1;
    chunk.dh_init1 = r;

    m.magic = magic;
    m.chunks.push_back(chunk);

    send(m, to);
}

void send(magic_t magic, dh_init2_chunk& r, link_endpoint const& to)
{
    key_message m;
    key_chunk chunk;

    chunk.type = key_chunk_type::dh_init2;
    chunk.dh_init2 = r;

    m.magic = magic;
    m.chunks.push_back(chunk);

    send(m, to);
}

void send(magic_t magic, dh_response1_chunk& r, link_endpoint const& to)
{
    key_message m;
    key_chunk chunk;

    chunk.type = key_chunk_type::dh_response1;
    chunk.dh_response1 = r;

    m.magic = magic;
    m.chunks.push_back(chunk);

    send(m, to);
}

byte_array
send(magic_t magic, dh_response2_chunk& r, link_endpoint const& to)
{
    key_message m;
    key_chunk chunk;

    chunk.type = key_chunk_type::dh_response2;
    chunk.dh_response2 = r;

    m.magic = magic;
    m.chunks.push_back(chunk);

    return send(m, to);
}

} // anonymous namespace

//=================================================================================================
// key_responder
//=================================================================================================

key_responder::key_responder(shared_ptr<host> host, magic_t magic)
    : link_receiver(*host, magic)
    , host_(host)
{}

bool key_responder::is_initiator_acceptable(link_endpoint const& initiator_ep,
            byte_array/*peer_id?*/ const& initiator_eid, byte_array const& user_data)
{
    return true;
}

/**
 * Compute HMAC challenge cookie for DH.
 */
byte_array
key_responder::calc_dh_cookie(shared_ptr<ssu::negotiation::dh_hostkey_t> hostkey,
    byte_array const& responder_nonce,
    byte_array const& initiator_hashed_nonce,
    ssu::link_endpoint const& src)
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

    return sha256::keyed_hash(hostkey->hmac_secret_key_, data);
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
            case ssu::negotiation::key_chunk_type::dh_response2:
                return got_dh_response2(*chunk.dh_response2, src);//key_initiator method?
            default:
                logger::warning() << "Unknown negotiation chunk type " << uint32_t(chunk.type);
                break;
        }
    }
};

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

    logger::debug() << "Send dh_response1 to " << src;
    send(magic(), response, src);
    // retransmit_timer_.restart();
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

    assert(initiator->initiator_hashed_nonce_ == data.initiator_hashed_nonce);
    assert(initiator->channel_ != nullptr);

    logger::debug() << "Got dh_response1";

    if (data.group >= dh_group_type::dh_group_max)
        return warning("invalid group type");
    if (data.group < initiator->dh_group_)
        return warning("key group less than required minimum");

    if (data.key_min_length != 128/8 and data.key_min_length != 192/8 and data.key_min_length != 256/8)
        return warning("invalid minimum AES key length");
    if (data.key_min_length < initiator->key_min_length_)
        return warning("key length less than required minimum");

    // If the group changes or our public key expires, revert to dh_init1 phase.
    if (initiator->dh_group_ != data.group)
    {
        initiator->dh_group_ = data.group;
        return initiator->send_dh_init1();
    }
    shared_ptr<dh_hostkey_t> hostkey(host_->get_dh_key(data.group));
    if (!hostkey or initiator->initiator_public_key_ != hostkey->public_key_)
        return initiator->send_dh_init1();

    // Always use the latest responder parameters received,
    // even if we've already received an R1 response.
    // XXX to be really DoS-protected from active attackers,
    // we should cache some number of the last R1 responses we get
    // until we receive a valid R2 response with the correct identity.
    initiator->key_min_length_ = data.key_min_length;
    initiator->responder_nonce_ = data.responder_nonce;
    initiator->responder_public_key_ = data.responder_dh_public_key;
    initiator->responder_challenge_cookie_ = data.responder_challenge_cookie;
    // XX ignore any public responder identity in the dh_response1 for now.

    initiator->shared_master_secret_ = hostkey->calc_key(data.responder_dh_public_key);

    // Sign the key parameters to prove our identity
    byte_array signature_hash = calc_signature_hash(initiator->dh_group_,
        initiator->key_min_length_, initiator->initiator_hashed_nonce_,
        initiator->responder_nonce_, initiator->initiator_public_key_,
        initiator->responder_public_key_, byte_array(/*no eid yet*/));
    byte_array signature = host_->host_identity().sign(signature_hash);

    // Build encrypted part of init2 message.
    initiator_identity_chunk iic;
    iic.initiator_channel_number = initiator->channel_->local_channel();
    iic.initiator_eid = host_->host_identity().id().id();
    iic.responder_eid = byte_array(); // XXX
    iic.initiator_id_public_key = host_->host_identity().public_key();
    iic.initiator_signature = signature;
    iic.user_data_in = initiator->user_data_in_;

    byte_array encrypted_initiator_info;
    {
        byte_array_owrap<flurry::oarchive> write(encrypted_initiator_info);
        write.archive() << iic;
    }

    // Encrypt it with AES-256-CBC
    byte_array enc_key = calc_key(initiator->shared_master_secret_,
        initiator->initiator_hashed_nonce_,
        initiator->responder_nonce_,
        '1', 256/8);

    encrypted_initiator_info = aes_256_cbc(aes_256_cbc::type::encrypt, enc_key).encrypt(encrypted_initiator_info);

    // Authenticate it with HMAC-SHA256-128
    byte_array mac_key = calc_key(initiator->shared_master_secret_,
        initiator->initiator_hashed_nonce_,
        initiator->responder_nonce_,
        '2', 256/8);

    // @todo: wrap this into a hmac_calc_append() kind of helper
    crypto::hash hmac(mac_key.as_vector());
    int hmac_size = crypto::HMACLEN;
    hmac.update(encrypted_initiator_info.as_vector());
    crypto::hash::value result;
    hmac.finalize(result);

    encrypted_initiator_info.append(result);

    initiator->encrypted_identity_info_ = encrypted_initiator_info;

    initiator->send_dh_init2();
    initiator->retransmit_timer_.start(async::timer::retry_min);
}

/**
 * We got init2, this means the init1/response1 phase might have been done.
 */
void key_responder::got_dh_init2(const dh_init2_chunk& data, const link_endpoint& src)
{
    logger::debug() << "Got dh_init2";

    // We'll need the originator's hashed nonce as well...
    byte_array initiator_hashed_nonce = sha256::hash(data.initiator_nonce);

    if (data.key_min_length != 128/8 and data.key_min_length != 192/8 and data.key_min_length != 256/8)
        return warning("invalid minimum AES key length");

    // Find the appropriate host key
    shared_ptr<dh_hostkey_t> hostkey(host_->get_dh_key(data.group)); // get or generate a host key
    if (!hostkey or data.responder_dh_public_key != hostkey->public_key_)
    {
        // Key mismatch, probably due to a timeout and key change.
        // Send a new dh_response1 instead of response2.
        logger::debug() << "Received dh_init2 with incorrect public key";

        dh_init1_chunk init;
        init.group = data.group;
        init.key_min_length = data.key_min_length;
        init.initiator_hashed_nonce = initiator_hashed_nonce;
        init.initiator_dh_public_key = data.initiator_dh_public_key;
        init.responder_eid.clear();

        return got_dh_init1(init, src);
    }

    // See if we've already responded to this particular dh_init2 - if so,
    // just return our previous cached response.
    // Use hhkr as the index, as per the JFK spec.
    if (hostkey->r2_cache_.find(data.responder_challenge_cookie) != hostkey->r2_cache_.end())
    {
        logger::debug() << "Received duplicate dh_init2 packet, replying with cached response.";
        src.send(hostkey->r2_cache_[data.responder_challenge_cookie]);
        return;
    }

    // Verify the challenge hash
    if (data.responder_challenge_cookie !=
        calc_dh_cookie(hostkey, data.responder_nonce, initiator_hashed_nonce, src))
    {
        logger::debug() << "Received dh_init2 with bad challenge hash, dropping.";
        return; // Just drop the bad dh_init2
    }

    //----------------------------------
    // Compute the shared master secret
    //----------------------------------
    logger::debug() << "Computing master secret.";

    byte_array master_secret = hostkey->calc_key(data.initiator_dh_public_key);

    // Check and strip the MAC field on the encrypted identity
    byte_array mac_key = calc_key(master_secret, initiator_hashed_nonce, data.responder_nonce, '2', 256/8);

    // For when we're tight on speed, the aes-256-ocb might be a better option:
    // http://www.cs.ucdavis.edu/~rogaway/ocb/news/code/ocb.c
    // http://www.cs.ucdavis.edu/~rogaway/ocb/news/code/ae.h
    // These are patented in US, but there's a free license for open source software.

    logger::debug() << "Verifying HMAC.";

    // @todo: wrap this into a hmac_verify() kind of helper
    crypto::hash hmac(mac_key.as_vector());
    int hmac_size = crypto::HMACLEN;
    byte_array block = data.initiator_info;
    block.resize(block.size() - hmac_size);
    hmac.update(block.as_vector());
    crypto::hash::value result;
    hmac.finalize(result);

    byte_array expected_hmac;
    expected_hmac.resize(hmac_size);
    std::copy(data.initiator_info.end() - hmac_size,
              data.initiator_info.end(),
              expected_hmac.begin());

    if (byte_array(result) != expected_hmac)
    {
        logger::warning() << "Received dh_init2 with bad initiator identity MAC.";
        return; // XXX generate cached error response instead
    }

    logger::debug() << "Decrypting initiator info.";

    // Decrypt it with AES-256-CBC
    byte_array enc_key = calc_key(master_secret, initiator_hashed_nonce, data.responder_nonce, '1', 256/8);
    byte_array initiator_info = aes_256_cbc(aes_256_cbc::type::decrypt, enc_key).decrypt(data.initiator_info);

    // Decode the identity information
    byte_array_iwrap<flurry::iarchive> read(initiator_info);
    initiator_identity_chunk iic;
    read.archive() >> iic;

    if (iic.initiator_channel_number == 0) {
        logger::debug() << "Received dh_init2 with bad identity info.";
        return; // XXX generate cached error response instead
    }

    // Check that the initiator is someone we want to talk with!
    if (!is_initiator_acceptable(src, iic.initiator_eid, iic.user_data_in))
    {
        logger::warning() << "Rejecting dh_init2 due to is_initiator_acceptable()";
        return; // XXX generate cached error response instead
    }

    // Check that the initiator actually wants to talk with us
    byte_array host_id = host_->host_identity().id().id();
    byte_array responder_eid = iic.responder_eid;
    if (responder_eid.is_empty())
    {
        responder_eid = host_id;
    }
    else if (responder_eid != host_id)
    {
        logger::warning() << "Received dh_init2 from initiator looking for someone else.";
        return; // XXX generate cached error response instead
    }

    // Verify the initiator's identity
    identity initiator_id(iic.initiator_eid);
    if (!initiator_id.set_key(iic.initiator_id_public_key))
    {
        logger::warning() << "Received dh_init2 with bad initiator public key.";
        return; // XXX generate cached error response instead
    }

    byte_array signature_hash = calc_signature_hash(data.group,
        data.key_min_length, initiator_hashed_nonce,
        data.responder_nonce, data.initiator_dh_public_key,
        data.responder_dh_public_key, iic.responder_eid);

    if (!initiator_id.verify(signature_hash, iic.initiator_signature))
    {
        logger::warning() << "Received dh_init2 with bad initiator signature.";
        return; // XXX generate cached error response instead
    }

    logger::debug() << "Authenticated initiator with id " << peer_id(iic.initiator_eid)
        << " at " << src;

    // Everything looks good - setup a channel and produce dh_response2.
    byte_array user_data_out;

    channel* chan = create_channel(src, iic.initiator_eid, iic.user_data_in, user_data_out);
    if (!chan)
    {
        logger::debug() << "Rejecting dh_init2 due to null return from create_channel()";
        return; // XXX generate cached error response instead
    }
    assert(chan->is_bound());
    assert(!chan->is_active());

    // *** Should be no failures after this point. ***

    // Sign the key parameters to prove our own identity
    signature_hash = calc_signature_hash(data.group, data.key_min_length, initiator_hashed_nonce,
        data.responder_nonce, data.initiator_dh_public_key, data.responder_dh_public_key,
        iic.initiator_eid);
    byte_array responder_signature = host_->host_identity().sign(signature_hash);

    // Build the part of the dh_response2 message to be encrypted.
    // (XX should we include anything for the 'sa' in the JFK spec?)
    responder_identity_chunk resp_ic;
    resp_ic.responder_channel_number = chan->local_channel();
    resp_ic.responder_eid = responder_eid;
    resp_ic.responder_id_public_key = host_->host_identity().public_key();
    resp_ic.responder_signature = responder_signature;
    resp_ic.user_data_out = user_data_out;

    byte_array encrypted_responder_info;
    {
        byte_array_owrap<flurry::oarchive> write(encrypted_responder_info);
        write.archive() << resp_ic;
    }

    // Encrypt and authenticate our identity
    encrypted_responder_info = aes_256_cbc(aes_256_cbc::type::encrypt, enc_key).encrypt(encrypted_responder_info);

    // @todo: wrap this into a hmac_calc_append() kind of helper
    crypto::hash hmac2(mac_key.as_vector());
    hmac2.update(encrypted_responder_info.as_vector());
    crypto::hash::value result2;
    hmac2.finalize(result2);

    encrypted_responder_info.append(result2);

    // Build, send, and cache our dh_response2.
    dh_response2_chunk response;
    response.initiator_hashed_nonce = initiator_hashed_nonce;
    response.responder_info = encrypted_responder_info;

    logger::debug() << "Send dh_response2 to " << src;
    byte_array pkt = send(magic(), response, src);

    hostkey->r2_cache_.insert(make_pair(data.responder_challenge_cookie, pkt));

    // Set up the armor for the new channel
    byte_array tx_enc_key = calc_key(master_secret, data.responder_nonce, initiator_hashed_nonce, 'E', 128/8);
    byte_array tx_mac_key = calc_key(master_secret, data.responder_nonce, initiator_hashed_nonce, 'A', 256/8);
    byte_array rx_enc_key = calc_key(master_secret, initiator_hashed_nonce, data.responder_nonce, 'E', 128/8);
    byte_array rx_mac_key = calc_key(master_secret, initiator_hashed_nonce, data.responder_nonce, 'A', 256/8);

    // @todo: where did make_unique go?
    chan->set_armor(unique_ptr<aes_armor>(new aes_armor(tx_enc_key, tx_mac_key, rx_enc_key, rx_mac_key)));

    // Set up the new channel IDs
    byte_array tx_chan_id = calc_key(master_secret, data.responder_nonce, initiator_hashed_nonce, 'I', 128/8);
    byte_array rx_chan_id = calc_key(master_secret, initiator_hashed_nonce, data.responder_nonce, 'I', 128/8);
    chan->set_channel_ids(tx_chan_id, rx_chan_id);

    // Let the ball roll
    chan->set_remote_channel(iic.initiator_channel_number);
    chan->start(false);
}

void key_responder::got_dh_response2(const dh_response2_chunk& data, const link_endpoint& src)
{
    shared_ptr<key_initiator> initiator = host_->get_initiator(data.initiator_hashed_nonce);
    if (!initiator)
        return warning("Got dh_response2 for unknown dh_init2");
    if (initiator->is_done())
        return warning("Got duplicate dh_response2 for completed initiator");
    if (initiator->shared_master_secret_.is_empty())
        return warning("Received dh_response2 ahead of dh_response1!");

    assert(initiator->initiator_hashed_nonce_ == data.initiator_hashed_nonce);
    assert(initiator->channel_ != nullptr);

    logger::debug() << "Got dh_response2";

    // Make sure our host key hasn't expired in the meantime
    // XXX but reverting here leaves the responder with a hung channel! <---
    shared_ptr<dh_hostkey_t> hostkey(host_->get_dh_key(initiator->dh_group_));
    if (!hostkey or initiator->initiator_public_key_ != hostkey->public_key_)
        return initiator->send_dh_init1();

    // Check and strip the MAC field on the responder's encrypted identity
    byte_array mac_key = calc_key(initiator->shared_master_secret_, initiator->initiator_hashed_nonce_,
        initiator->responder_nonce_, '2', 256/8);

    logger::debug() << "Verifying HMAC.";

    // @todo: wrap this into a hmac_verify() kind of helper
    crypto::hash hmac(mac_key.as_vector());
    int hmac_size = crypto::HMACLEN;
    byte_array block = data.responder_info;
    block.resize(block.size() - hmac_size);
    hmac.update(block.as_vector());
    crypto::hash::value result;
    hmac.finalize(result);

    byte_array expected_hmac;
    expected_hmac.resize(hmac_size);
    std::copy(data.responder_info.end() - hmac_size,
              data.responder_info.end(),
              expected_hmac.begin());

    if (byte_array(result) != expected_hmac)
    {
        logger::warning() << "Received dh_response2 with bad responder identity MAC.";
        return;
    }

    logger::debug() << "Decrypting responder info.";

    // Decrypt it with AES-256-CBC
    byte_array enc_key = calc_key(initiator->shared_master_secret_, initiator->initiator_hashed_nonce_,
        initiator->responder_nonce_, '1', 256/8);

    byte_array responder_info = aes_256_cbc(aes_256_cbc::type::decrypt, enc_key).decrypt(data.responder_info);

    // Decode the identity information
    byte_array_iwrap<flurry::iarchive> read(responder_info);
    responder_identity_chunk resp_ic;
    read.archive() >> resp_ic;

    if (resp_ic.responder_channel_number == 0 or resp_ic.responder_eid.is_empty())
    {
        logger::debug() << "Received dh_response2 with bad responder identity info.";
        return;
    }

    // Make sure the responder is who we actually wanted to talk to
    if (!initiator->remote_id_.is_empty() and resp_ic.responder_eid != initiator->remote_id_)
    {
        logger::warning() << "Received dh_response2 from responder with wrong identity.";
        return;
    }

    // Verify the initiator's identity
    identity responder_id(resp_ic.responder_eid);
    if (!responder_id.set_key(resp_ic.responder_id_public_key))
    {
        logger::warning() << "Received dh_response2 with bad responder public key.";
        return;
    }

    byte_array signature_hash = calc_signature_hash(initiator->dh_group_,
        initiator->key_min_length_, initiator->initiator_hashed_nonce_,
        initiator->responder_nonce_, initiator->initiator_public_key_,
        initiator->responder_public_key_, host_->host_identity().id().id());

    if (!responder_id.verify(signature_hash, resp_ic.responder_signature))
    {
        logger::warning() << "Received dh_response2 with bad responder signature.";
        return;
    }

    logger::debug() << "Authenticated responder with id " << peer_id(resp_ic.responder_eid)
        << " at " << src;

    // Set up new channel's armor
    byte_array tx_enc_key = calc_key(initiator->shared_master_secret_,
                                     initiator->initiator_hashed_nonce_,
                                     initiator->responder_nonce_, 'E', 128/8);
    byte_array tx_mac_key = calc_key(initiator->shared_master_secret_,
                                     initiator->initiator_hashed_nonce_,
                                     initiator->responder_nonce_, 'A', 256/8);
    byte_array rx_enc_key = calc_key(initiator->shared_master_secret_,
                                     initiator->responder_nonce_,
                                     initiator->initiator_hashed_nonce_, 'E', 128/8);
    byte_array rx_mac_key = calc_key(initiator->shared_master_secret_,
                                     initiator->responder_nonce_,
                                     initiator->initiator_hashed_nonce_, 'A', 256/8);

    initiator->channel_->set_armor(unique_ptr<aes_armor>(new aes_armor(tx_enc_key, tx_mac_key, rx_enc_key, rx_mac_key)));

    // Set up the new channel IDs
    byte_array tx_chan_id = calc_key(initiator->shared_master_secret_, initiator->initiator_hashed_nonce_,
                                     initiator->responder_nonce_, 'I', 128/8);
    byte_array rx_chan_id = calc_key(initiator->shared_master_secret_, initiator->responder_nonce_,
                                     initiator->initiator_hashed_nonce_, 'I', 128/8);
    initiator->channel_->set_channel_ids(tx_chan_id, rx_chan_id);

    // Finish flow setup
    initiator->channel_->set_remote_channel(resp_ic.responder_channel_number);

    // Our job is done
    initiator->channel_->start(true);
    initiator->done();
}

//=================================================================================================
// key_initiator
//=================================================================================================

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
    initiator_hashed_nonce_ = sha256::hash(initiator_nonce_);
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
    init.group = dh_group_;
    init.key_min_length = key_min_length_;
    init.initiator_nonce = initiator_nonce_;
    init.responder_nonce = responder_nonce_;
    init.initiator_dh_public_key = initiator_public_key_;
    init.responder_dh_public_key = responder_public_key_;
    init.responder_challenge_cookie = responder_challenge_cookie_;
    init.initiator_info = encrypted_identity_info_;

    send(magic(), init, target_);
}

} // negotiation namespace

//=================================================================================================
// key_host_state
//=================================================================================================

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

} // ssu namespace
