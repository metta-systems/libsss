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

class byte_array;

namespace ssu {
namespace crypto {

class secure_hash;

/**
 * Abstract base class for public-key cryptographic signing methods.
 *
 * NOTE:
 * A key should not be reused for multiple purposes; that may open up various subtle attacks.
 *
 * For instance, if you have an RSA private/public key pair, you should not both use it 
 * for encryption (encrypt with the public key, decrypt with the private key) and
 * for signing (sign with the private key, verify with the public key): pick a single purpose 
 * and use it for just that one purpose. If you need both abilities, generate two keypairs, 
 * one for signing and one for encryption/decryption.
 *
 * from http://security.stackexchange.com/questions/2202/lessons-learned
 */
class sign_key
{
public:
    enum key_type {
        invalid = 0,            ///< Uninitialized or invalid key.
        public_only = 1,        ///< Public key only.
        public_and_private = 2, ///< Public and private key.
    };

    virtual ~sign_key();

    /**
     * Get the type of this key (public or private).
     * @return Key category (public, public and private, invalid).
     */
    inline key_type type() const { return type_; }

    /**
     * Get the short hash ID of this public or private key,
     * which is usable only for uniquely identifying the key.
     */
    virtual byte_array id() const = 0;

    /**
     * Get binary-encoded public key.
     * @return Serialized public key data.
     */
    virtual byte_array public_key() const = 0;

    /**
     * Get binary-encoded public and private keys.
     * @return Serialized public and private key data.
     */
    virtual byte_array private_key() const = 0;

    /**
     * Generate signature
     * @param  digest Digest of the message to be signed.
     * @return        Message digital signature.
     */
    virtual byte_array sign(byte_array const& digest) const = 0;

    /**
     * Verify a signature
     * @param  digest    Digest of the signed message.
     * @param  signature Message signature.
     * @return           true if signature verification succeeded, false otherwise.
     */
    virtual bool verify(byte_array const& digest, byte_array const& signature) const = 0;

protected:
    sign_key();

    inline void set_type(key_type t) { type_ = t; }

private:
    key_type type_;
};

} // crypto namespace
} // ssu namespace
