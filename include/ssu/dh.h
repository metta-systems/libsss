//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <unordered_map>
#include "arsenal/byte_array.h"
#include "ssu/timer.h"
#include "ssu/negotiation/key_message.h"

namespace ssu {

class host;

namespace negotiation {

class key_responder;
class key_initiator;

/**
 * Replace this with short-term keys for key exchange.
 */
class shortterm_key_t
{
    std::shared_ptr<ssu::host> host_;    // Host to which this key is attached.
    ssu::async::timer expiration_timer_; // Expiration timer for key.
    byte_array public_key_;
    byte_array secret_key_;

    /**
     * Hash table of cached R2 responses made using this key, for replay protection.
     */
    std::unordered_map<byte_array, byte_array> r2_cache_;

    /**
     * Expire the key.
     */
    void timeout();

    /**
     * Compute a shared master secret from our private key and other_public_key.
     */
    byte_array calc_key(byte_array const& other_public_key);

public:
    shortterm_key_t(std::shared_ptr<ssu::host> host);
    ~shortterm_key_t();
};

} // namespace negotiation

/**
 * Host state mixin related to DH key exchange.
 */
class shortterm_keys_host_state
{
    std::array<std::shared_ptr<negotiation::dh_hostkey_t>, /*dh_group_max*/3> dh_keys_;

    std::shared_ptr<negotiation::dh_hostkey_t>
    internal_generate_dh_key(negotiation::dh_group_type group, DH *(*groupfunc)());

public:
    shortterm_keys_host_state() = default;
    virtual ~shortterm_keys_host_state();

    std::shared_ptr<negotiation::dh_hostkey_t> get_dh_key(negotiation::dh_group_type group);
    void clear_dh_key(negotiation::dh_group_type group);

    virtual std::shared_ptr<host> get_host() = 0;
};

} // namespace ssu
