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
 */
class sign_key
{
public:
    enum key_type {
        invalid = 0,
        public_only = 1,
        public_and_private = 2,
    };

    /// Get the type of this key (public or private).
    inline key_type type() const { return type_; }

    /// Get the short hash ID of this public or private key,
    /// which is usable only for uniquely identifying the key.
    virtual byte_array id() const = 0;

    /// Get this Ident's binary-encoded public key.
    virtual byte_array public_key() const = 0;

    /// Get this Ident's binary-encoded private key.
    virtual byte_array private_key() const = 0;

    /// Create a new SecureHash object suitable for hashing messages
    /// to be signed using this identity's private key.
    virtual std::unique_ptr<secure_hash> create_hash() const = 0;

    /// Generate signature
    virtual byte_array sign(byte_array const& digest) const = 0;

    /// Verify a signature
    virtual bool verify(byte_array const& digest, byte_array const& signature) const = 0;

    virtual ~sign_key();

protected:
    sign_key();

    inline void set_type(key_type t) { type_ = t; }

private:
    key_type type_;
};

} // crypto namespace
} // ssu namespace
