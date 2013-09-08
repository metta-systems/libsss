//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

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

    bool has_private_key() const;
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
