//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
// #include <openssl/err.h>
// #include <openssl/obj_mac.h>
#include <openssl/sha.h>
#include "crypto/rsa160_key.h"
#include "crypto/utils.h"
#include "crypto.h"
#include "byte_array.h"
#include "msgpack_oarchive.h"
#include "archive_helper.h"

namespace ssu {
namespace crypto {

rsa160_key::rsa160_key(RSA *rsa)
    : rsa_(rsa)
{}

/// @todo
rsa160_key::rsa160_key(byte_array const& key)
{}

rsa160_key::rsa160_key(int bits, unsigned e)
{
    if (bits == 0) {
        bits = 1024;
    }
    if (e == 0) {
        e = 65537;
    }
    if (!(e % 2)) {
        ++e; // e must be odd
    }

    // Generate a new RSA key given those parameters
    rsa_ = RSA_generate_key(bits, e, NULL, NULL);

    assert(rsa_);
    assert(rsa_->d);

    set_type(public_and_private);
}

rsa160_key::~rsa160_key()
{
    if (rsa_) {
        RSA_free(rsa_);
        rsa_ = NULL;
    }
}

byte_array 
rsa160_key::id() const
{
    if (type() == invalid)
        return byte_array();

    crypto::hash::value hash;
    crypto::hash md;
    md.update(public_key().as_vector()).finalize(hash);

    byte_array id(hash);
    // Only return 160 bits of key identity information,
    // because this method's security may be limited by the SHA-1 hash
    // used in the RSA-OAEP padding process.
    id.resize(160/8);

    return id;
}

byte_array
rsa160_key::public_key() const
{
    byte_array data;
    {
        byte_array_owrap<msgpack_oarchive> w(data);

        // Write the public part of the key
        // bool has_private_key = false;
        // w.archive() << crypto::utils::bn2ba(rsa_->n) << rsa_->e << false;
    }
    return data;
}

byte_array
rsa160_key::private_key() const
{
    byte_array data;
    {
    }
    return data;
}

struct secure_hash {}; // @fixme temp

std::unique_ptr<secure_hash>
rsa160_key::create_hash() const
{
    return std::unique_ptr<secure_hash>(nullptr); //new sha256_hash();
}

byte_array
rsa160_key::sign(byte_array const& digest) const
{
    if (type() == invalid)
        return byte_array();
    assert(digest.size() == SHA256_DIGEST_LENGTH);

    byte_array signature;
    unsigned siglen = RSA_size(rsa_);
    signature.resize(siglen);

    return signature;
}

bool
rsa160_key::verify(byte_array const& digest, byte_array const& signature) const
{
    return false;
}

void
rsa160_key::dump() const
{}

} // crypto namespace
} // ssu namespace
