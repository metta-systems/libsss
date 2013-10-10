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
#include "settings_provider.h"

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

    identity ident(id);
    ident.set_key(key);

    return ident;
}

identity::scheme identity::key_scheme() const
{
    if(id_.is_empty())
        return scheme::null;

    return scheme(id_.id()[0] >> 3);
}

identity identity::from_endpoint(endpoint const& ep)
{
    byte_array buf;

    if (ep.address().is_v4())
        buf.append({scheme::ipv4});
    else
        buf.append({scheme::ipv6});

    buf.append(byte_array::wrap((const char*)ep.data(), ep.size()));
    // @todo Append port
    // buf.append(ep.port());

    identity ident(buf);
    return ident;
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

void identity_host_state::set_host_identity(identity const& ident)
{
    if (ident.has_private_key()) {
        logger::warning() << "Using a host identity with no private key!";
    }
    host_identity_ = ident;
}

void identity_host_state::init_identity(settings_provider* settings)
{
    if (host_identity_.has_private_key())
        return; // Already initialized.
    if (!settings)
    {
        host_identity(); // No persistence available.
        return;
    }

    // Find and decode the host's existing key, if any.
    byte_array id = settings->get_byte_array("id");
    byte_array key = settings->get_byte_array("key");

    if (!id.is_empty() and !key.is_empty())
    {
        host_identity_.set_id(id);
        if (host_identity_.set_key(key) && host_identity_.has_private_key())
            return;     // Success
    }

    logger::warning() << "Invalid host identity in settings: generating new identity";

    // Generate a new key pair
    host_identity_ = identity::generate();

    // Save it in our host settings
    settings->set("id", host_identity_.id().id());
    settings->set("key", host_identity_.private_key());
    settings->sync();
}

} // ssu namespace
