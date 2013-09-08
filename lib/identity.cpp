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

identity identity::generate(scheme sch, int bits)
{
    crypto::sign_key* key;
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

identity identity_host_state::host_identity()
{
    if (!host_identity_.has_private_key())
    {
        host_identity_ = identity::generate();
    }
    return host_identity_;
}

} // ssu namespace
