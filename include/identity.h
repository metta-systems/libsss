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
#include "crypto/secure_hash.h"

class settings_provider;

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
     * Non-cryptographic identifiers cannot have signing keys.
     */
    static identity from_endpoint(endpoint const& ep);

    // @todo:
    // from_mac_address()
    // from_ip_address(address_v4)
    // from_ip_address(address_v6)

    /**
     * Get this identity's short binary EID.
     * @return the binary identifier as a byte_array.
     */
    peer_id id() const {
        return id_;
    }
    /**
     * Set the identity's short binary EID.
     * Clears any associated key information.
     * @param id the binary identifier.
     */
    inline void set_id(peer_id const& id) {
        id_ = id;
        clear_key();
    }

    /**
     * Determine the scheme number this ID uses.
     * @return the scheme number.
     */
    scheme key_scheme() const;

    /**
     * Determine whether this identifier contains an associated key
     * usable for signature verification.
     * @return true if this identity contains a public key.
     */
    inline bool has_key() const {
        return key_ and key_->type() != crypto::sign_key::invalid;
    }

    /**
     * Determine whether this identifier contains a private key
     * usable for both signing and verification.
     * @return true if this identity contains a private key.
     */
    inline bool has_private_key() const {
        return key_ and key_->type() == crypto::sign_key::public_and_private;
    }

    /**
     * Check for the distinguished "null identity".
     * @return true if this is a null identity.
     */
    inline bool is_null() const {
        return key_scheme() == scheme::null;
    }

    inline bool is_ip_key_scheme() const {
        return key_scheme() == identity::scheme::ipv4 or key_scheme() == identity::scheme::ipv6;
    }

    /**
     * Get this identity's binary-encoded public key.
     * @return the key serialized into a byte_array.
     */
    byte_array public_key() const;

    /**
     * Get this identity's binary-encoded public and private keys.
     * @return the key serialized into a byte_array.
     */
    byte_array private_key() const;

    /**
     * Set the public or private key associated with this identity.
     * Note: set the identity first, as it clears the key.
     * @param key the binary-encoded public or private key.
     * @return true if the encoded key was recognized, valid,
     *     and the correct key for this identifier.
     */
    bool set_key(byte_array const& key);

    void clear_key();

    /**
     * Create a new secure_hash object suitable for hashing messages
     * to be signed using this identity's private key.
     * @return the new secure_hash object.
     */
    inline std::unique_ptr<secure_hash> create_hash() const {
        assert(key_);
        return key_->create_hash();
    }

    /**
     * Hash a block of data using this identity scheme's hash function.
     * This is just a convenience function based on create_hash().
     * @param data a pointer to the data to hash.
     * @param len the number of bytes to hash.
     * @return the resulting hash, in a byte_array.
     * @see create_hash
     */
    byte_array hash(char const* data, int len) const;

    /**
     * Hash a byte_array using this identity scheme's hash function.
     * This is just a convenience function based on create_hash().
     * @param data the byte_array to hash.
     * @return the resulting hash, in a byte_array.
     * @see create_hash
     */
    inline byte_array hash(byte_array const& data) const {
        return hash(data.const_data(), data.size());
    }

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
        assert(key_);
        return key_->verify(digest, sig);
    }
};

class identity_host_state
{
    identity host_identity_;
public:
    /**
     * Create if necessary and return the host's global cryptographic identity.
     * Generates a new identity and private key if one isn't already set.
     * @return the primary host identity.
     */
    identity host_identity();

    /**
     * Set host identity from the outside. Given identity must have a private key,
     * otherwise this host will be impossible to connect to or from.
     * @param ident Replacement host identity.
     */
    void set_host_identity(identity const& ident);

    /**
     * Initialize our primary host identity using a settings_provider for persistence.
     * Looks for the tags 'id' and 'key' in the provided @a settings, and if they contain
     * a valid host identity and private key, uses them to set the primary host identity.
     * Otherwise, generates a new primary host identity and encodes them into the provided
     * @a settings for future invocations of the application.
     *
     * This function does nothing if the primary host identity is already initialized and
     * contains a valid private key.
     *
     * If no settings registry is specified, generates a new primary identity if not already
     * generated.
     *
     * @param settings the settings registry to use for persistence.
     */
    void init_identity(settings_provider* settings);
};

} // ssu namespace
