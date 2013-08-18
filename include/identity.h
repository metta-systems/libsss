#pragma once

namespace ssu {

/** 
 * Represents an endpoint identifier and optionally an associated cryptographic signing key.
 *
 * Represents an SSU endpoint identifier (EID).
 * SSU uses EIDs in place of IP addresses to identify hosts or virtual endpoint identities
 * on a particular host (e.g., identites for specific user accounts on multiuser hosts).
 * An EID is a variable-length binary string of bytes, whose exact interpretation depends
 * on the scheme number embedded in the first 6 bits of each EID.
 * EIDs can represent both cryptographically self-certifying identifiers and legacy addresses
 * such as IP addresses and IP/port pairs.
 * Although EIDs are not usually intended to be seen by the user, they have a standard
 * filename/URL-compatible base64 text encoding, in which the first character
 * encodes the scheme number. @fixme
 */
class identity
{
public:
    static identity from_endpoint(endpoint const& ep);

    byte_array id() const;
};

} // ssu namespace
