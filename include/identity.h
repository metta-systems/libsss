//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <stdexcept>
#include "link.h"
#include "byte_array.h"
#include "peer_id.h"
#include "crypto/sign_key.h"

namespace ssu {

/** 
 * Represents an endpoint identifier and optionally an associated cryptographic signing key.
 *
 * SSU uses EIDs in place of IP addresses to identify hosts or virtual endpoint identities
 * on a particular host (e.g., identites for specific user accounts on multiuser hosts).
 * An EID is a variable-length binary string of bytes, whose exact interpretation depends
 * on the scheme number embedded in the first 5 bits of each EID.
 * EIDs can represent both cryptographically self-certifying identifiers and legacy addresses
 * such as IP addresses and IP/port pairs.
 * Although EIDs are not usually intended to be seen by the user, they have a standard
 * filename/URL-compatible base32 text encoding, in which the first character
 * encodes the scheme number.
 */
class identity
{
    crypto::sign_key* key_{0};
    peer_id           id_;

public:
    class bad_key final : public std::runtime_error {
    public:
        explicit inline bad_key() : std::runtime_error("bad identity key") {}
    };

    /**
     * Endpoint identifier scheme numbers.
     * The scheme number occupies the top 5 bits in any EID,
     * making the EID's scheme easily recognizable
     * via the first character in its base32 representation.
     */
    enum scheme {
        null = 0,     ///< Reserved for the "null" identity.

        // Non-cryptographic legacy address schemes
        mac   = 1,    ///< IEEE MAC address
        ipv4  = 2,    ///< IPv4 address with optional port
        ipv6  = 3,    ///< IPv6 address with optional port

        // Cryptographic identity schemes
        dsa160  = 10, ///< DSA with SHA-256, yielding 160-bit IDs
        rsa160  = 11, ///< RSA with SHA-256, yielding 160-bit IDs
    };

    /**
     * Create an invalid identity.
     */
    identity() = default;

    /**
     * Create an identity with a given binary identifier.
     * @param id the binary identifier.
     */
    identity(byte_array const& id);

    /**
     * Create an identity with a given binary identifier.
     * @param id the binary peer identifier.
     */
    identity(peer_id const& id);

    /**
     * Create an identity with a binary identifier and corresponding key.
     * Throws bad_key if key data is invalid.
     * @param id the binary identifier.
     * @param key the binary representation of the key associated with the identifier.
     */
    identity(byte_array const& id, byte_array const& key);

    /**
     * Generate a new identity with unique private key, using reasonable default parameters.
     * @param scheme the signing scheme to use.
     * @param bits the desired key strength in bits, or 0 to use the selected scheme's default.
     * @return the generated identity.
     */
    static identity generate(scheme sch = rsa160, int bits = 0);

    /**
     * Construct a non-cryptographic EID from an endpoint IP address.
     */
    static identity from_endpoint(endpoint const& ep);

    /**
     * Get this identity's EID part.
     */
    peer_id id() const;

    scheme key_scheme() const;
    bool has_private_key() const;

    inline bool is_ip_key_scheme() const {
        return key_scheme() == identity::scheme::ipv4 or key_scheme() == identity::scheme::ipv6;
    }

    /// Get this identity's binary-encoded public key.
    byte_array public_key() const;

    /// Get this identity's binary-encoded private key.
    byte_array private_key() const;

    void clear_key();
    bool set_key(byte_array const& key);

    /**
     * Sign a message.
     * This identity must contain a valid private key.
     * @param digest the hash digest of the message to be signed.
     * @return the resulting signature, in a byte_array.
     */
    inline byte_array sign(byte_array const& digest) {
        assert(key_);
        return key_->sign(digest);
    }

    /**
     * Verify a signature.
     * This identity must contain a valid public key.
     * @param digest the hash digest of the signed message.
     * @param sig the signature to be verified.
     * @return true if signature verification succeeds.
     */
    inline bool verify(byte_array const& digest, byte_array const& sig) const {
        return key_->verify(digest, sig);
    }
};

class identity_host_state
{
    identity host_identity_;
public:
    /**
     * Create if necessary and return the host's cryptographic identity.
     */
    identity host_identity();
};

} // ssu namespace
