//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <memory>
#include "link.h"
#include "timer.h"
#include "crypto.h"
#include "negotiation/key_message.h"

namespace ssu {

class host;

namespace negotiation {

class dh_hostkey_t;

/**
 * This abstract base class manages the responder side of the key exchange.
 * It uses link_receiver interface as base to receive negotiation protocol control messages
 * and respond to incoming key exchange requests.
 *
 * It forwards received requests to a corresponding key initiator in the host state
 * (via host_->get_initiator()).
 */
class key_responder : public link_receiver
{
    std::shared_ptr<host> host_;

    void got_dh_init1(const dh_init1_chunk& data, const link_endpoint& src);
    void got_dh_response1(const dh_response1_chunk& data, const link_endpoint& src);
    void got_dh_init2(const dh_init2_chunk& data, const link_endpoint& src);
    void got_dh_response2(const dh_response2_chunk& data, const link_endpoint& src);

    byte_array
    calc_dh_cookie(std::shared_ptr<ssu::negotiation::dh_hostkey_t> hostkey,
        const byte_array& responder_nonce,
        const byte_array& initiator_hashed_nonce,
        const ssu::link_endpoint& src);

public:
    key_responder(std::shared_ptr<host> host);

    void receive(const byte_array& msg, const link_endpoint& src) override;
};

/**
 * Key initiator maintains host state with respect to initiated key exchanges.
 * One initiator keeps state about key exchange with one peer.
 */
class key_initiator
{
    std::shared_ptr<host> host_;
    const link_endpoint&  to_; ///< Remote endpoint we're trying to contact.
    peer_id               remote_id_;
    byte_array            user_info_; ///< Transparent user info block transmitted in init2 phase.
    magic_t               magic_{0};
    uint32_t              allowed_methods_{0}; ///< Bitwise set of below method flags

    enum methods {
        key_method_checksum = (1 << 0),
        key_method_aes      = (1 << 1),
    };

    /**
     * Current phase of the protocol negotiation.
     */
    enum class state {
        init1, init2, done
    } state_;

    ssu::async::timer retransmit_timer_;

    // Weak keyed checksum state

    uint32_t checksum_key_;
    byte_array responder_cookie_;

    // AES/SHA256 with DH key agreement

    ssu::negotiation::dh_group_type dh_group_;
    int key_min_length_{0};

    // Protocol state set up before sending init1
    byte_array                                 initiator_nonce_;
    boost::array<uint8_t, crypto::hash::size>  initiator_hashed_nonce_;
    byte_array                                 initiator_public_key_;

    // Set after receiving response1
    byte_array                                 responder_nonce_;
    byte_array                                 responder_public_key_;
    byte_array                                 responder_challenge_cookie_;
    byte_array                                 shared_master_secret_;
    // Encrypted and authenticated identity information
    byte_array                                 encrypted_identity_info_;
    
    void retransmit(bool fail);

protected:
    inline magic_t magic() const { return magic_; }

public:
    key_initiator(const link_endpoint& target);
    ~key_initiator();

    void initiate(); // Actually start init1 phase.

    inline ssu::negotiation::dh_group_type group() const { return dh_group; }
    inline bool is_done() const { return state_ == state::done; }

    void send_dh_init1();
    void send_dh_response1();
    void send_dh_init2();
    void send_dh_response2();
};

} // namespace negotiation

/**
 * Mixin for host state that manages the key exchange state.
 */
class key_host_state
{
    //std::unordered_map<chk_ep, std::shared_ptr<key_initiator>> chk_initiators;
    std::unordered_map<byte_array, std::shared_ptr<negotiation::key_initiator>> dh_initiators_;
    // std::unordered_multimap<endpoint, std::shared_ptr<key_initiator>> ep_initiators;

public:
    std::shared_ptr<negotiation::key_initiator> get_initiator(byte_array nonce);
};

} // namespace ssu
