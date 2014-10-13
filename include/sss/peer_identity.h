//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <stdexcept>
// #include <boost/asio.hpp> // @todo Include only header for boost::asio::ip::address
// #include "arsenal/byte_array.h"
#include "arsenal/proquint.h"
#include "comm/socket_endpoint.h"
// #include "krypto/sign_key.h"

class settings_provider;

namespace uia {

/**
 * Represents a cryptographically self-certifying endpoint identifier.
 * It is a curve25519 public key (256 bits).
 *
 * SSU uses EIDs in place of IP addresses to identify hosts or virtual endpoint identities
 * on a particular host (e.g., identites for specific user accounts on multiuser hosts).
 * Although EIDs are not usually intended to be seen by the user, they have a standard
 * filename/URL-compatible base32 text encoding. A 16-word proquint encoding is also possible.
 * Yet another way to exchange the EIDs between two users is QR-code.
 * User profile classes handle this, as they also attach profile information to generated QR-codes.
 */
class peer_identity
{
    std::string id_; // public key
    std::string private_key_;

public:
    /**
     * Exception thrown when invalid identity key is encountered.
     */
    class bad_key final : public std::runtime_error {
    public:
        explicit inline bad_key() : std::runtime_error("bad identity key") {}
    };

    /**
     * Create an invalid identity.
     */
    peer_identity() = default;

    /**
     * Create an identity with a given binary identifier.
     * @param id the binary identifier.
     */
    peer_identity(byte_array const& id);

    /**
     * Create an identity with a given proquint representation of binary identifier.
     * @param proquint the binary identifier in proquint text encoding.
     */
    inline peer_identity(std::string proquint)
        : peer_identity(byte_array(encode::from_proquint(proquint)))
    {}

    /**
     * Create an identity with a binary identifier and corresponding private key.
     * Throws bad_key if key data is invalid.
     * @param id the binary identifier.
     * @param key the binary representation of the key associated with the identifier.
     */
    peer_identity(byte_array const& id, byte_array const& key);

    /**
     * Generate a new cryptographic identity with unique private key, using reasonable
     * default parameters.
     * @return the generated identity.
     */
    static peer_identity generate();

    /**
     * Get this identity's short binary EID.
     * @return the binary identifier as a byte_array.
     */
    byte_array id() const {
        return id_;
    }

    /**
     * Set the identity's short binary EID.
     * Clears any associated key information.
     * @param id the binary identifier.
     */
    inline void set_id(byte_array const& id) {
        id_ = id;
        clear_key();
    }

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

    /**
     * Get this identity's binary-encoded public key.
     * @return the key serialized into a byte_array.
     */
    byte_array public_key() const;

    /**
     * Get this identity's binary-encoded public and private keys.
     * @return the key serialized into a byte_array.
     */
    byte_array secret_key() const;

    /**
     * Set the public or private key associated with this identity.
     * Note: set the identity first, as it clears the key.
     * @param key the binary-encoded public or private key.
     * @return true if the encoded key was recognized, valid,
     *     and the correct key for this identifier.
     */
    bool set_key(byte_array const& key);

    void clear_key();

    inline std::string to_string() const { return encode::to_proquint(id_); }
};

inline bool operator == (peer_identity const& a, peer_identity const& b) { return a.id() == b.id(); }
inline bool operator != (peer_identity const& a, peer_identity const& b) { return a.id() != b.id(); }

inline std::ostream& operator << (std::ostream& os, peer_identity const& id)
{
    return os << id.to_string();
}

/**
 * Host state mixin relevant to identity management.
 */
class identity_host_state
{
    peer_identity host_identity_;
public:
    /**
     * Create if necessary and return the host's global cryptographic identity.
     * Generates a new identity and private key if one isn't already set.
     * @return the primary host identity.
     */
    peer_identity host_identity();

    /**
     * Set host identity from the outside. Given identity must have a private key,
     * otherwise this host will be impossible to connect to or from.
     * @param ident Replacement host identity.
     */
    void set_host_identity(peer_identity const& ident);

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

} // uia namespace

namespace flurry {

inline flurry::oarchive& operator << (flurry::oarchive& oa, uia::peer_identity const& id)
{
    oa << id.id();
    return oa;
}

inline flurry::iarchive& operator >> (flurry::iarchive& ia, uia::peer_identity& id)
{
    byte_array i;
    ia >> i;
    id = i;
    return ia;
}

} // flurry namespace

// Hash specialization for peer_id
namespace std {

template<>
struct hash<uia::peer_identity> : public std::unary_function<uia::peer_identity, size_t>
{
    inline size_t operator()(uia::peer_identity const& a) const noexcept
    {
        return std::hash<byte_array>()(a.id());
    }
};

} // std namespace

