//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "identity.h"
#include "crypto/dsa160_key.h"
#include "crypto/rsa160_key.h"
#include "logging.h"

namespace ssu {

//=================================================================================================
// identity
//=================================================================================================

identity::identity(byte_array const& id)
    : id_(id)
{
}

identity::identity(peer_id const& id)
    : id_(id)
{}

identity::identity(byte_array const& id, byte_array const& key)
    : id_(id)
{
    if (!set_key(key))
        throw bad_key();
}

void identity::clear_key()
{
    delete key_;
    key_ = nullptr;
}

bool identity::set_key(byte_array const& key)
{
    clear_key();

    scheme ksch = key_scheme();
    switch (ksch) {
        case dsa160:
            key_ = new crypto::dsa160_key(key);
            break;
        case rsa160:
            key_ = new crypto::rsa160_key(key);
            break;
        default:
            logger::warning() << "Unknown identity key scheme " << ksch;
            return false;
    }

    // Check if decode succeeded.
    if (key_->type() == crypto::sign_key::key_type::invalid)
    {
        clear_key();
        return false;
    }

    // Verify that the supplied key actually matches the ID we have.
    // *** This is a crucial step for security! ***
    byte_array key_id = key_->id();
    key_id[0] = (key_id[0] & 7) | (ksch << 3); // replace top 5 bits of ID with scheme used

    if (key_id != id_)
    {
        clear_key();
        logger::warning() << "Attempt to set mismatching identity key!";
        return false;
    }

    return true;
}

identity identity::generate(scheme sch, int bits)
{
    crypto::sign_key* key{nullptr};
    switch (sch) {
        case dsa160:
            logger::debug() << "Generating new DSA160 sign key";
            key = new crypto::dsa160_key(bits);
            break;
        case rsa160:
            logger::debug() << "Generating new RSA160 sign key";
            key = new crypto::rsa160_key(bits);
            break;
        default:
            logger::fatal() << "Unsupported signing scheme " << sch;
    }

    byte_array id = key->id();
    id[0] = (id[0] & 7) | (sch << 3); // replace top 5 bits of ID with scheme used
    logger::debug() << "Generated key id " << id;

    identity ident;
    ident.key_ = key;
    ident.id_ = id;

    return ident;
}

identity::scheme identity::key_scheme() const
{
    assert(!id_.is_empty());
    return scheme(id_.id()[0] >> 3);
}

identity identity::from_endpoint(endpoint const& ep)
{
    identity ident;
    ident.id_ = byte_array();

    return ident;
}

peer_id identity::id() const
{
    return id_;
}

bool identity::has_private_key() const
{
    return key_ and key_->type() == crypto::sign_key::public_and_private;
}

byte_array identity::public_key() const
{
    if (!key_)
        return byte_array();
    return key_->public_key();
}

byte_array identity::private_key() const
{
    if (!key_)
        return byte_array();
    return key_->private_key();
}

//=================================================================================================
// identity_host_state
//=================================================================================================

identity identity_host_state::host_identity()
{
    if (!host_identity_.has_private_key())
    {
        host_identity_ = identity::generate();
    }
    return host_identity_;
}

} // ssu namespace
