//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "identity.h"

namespace ssu {

identity identity::from_endpoint(endpoint const& ep)
{
    return identity();
}

byte_array identity::id() const
{
    return byte_array();
}

} // ssu namespace
