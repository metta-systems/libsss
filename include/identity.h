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
    identity() {}
public:
    static identity from_endpoint(endpoint const& ep);

    byte_array id() const;
};

} // ssu namespace
