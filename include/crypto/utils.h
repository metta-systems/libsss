//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <openssl/dh.h>

class byte_array;

namespace ssu {
namespace crypto {
namespace utils {

// Little helper functions for BIGNUM to byte_array conversions.
BIGNUM* ba2bn(byte_array const& ba);
byte_array bn2ba(BIGNUM const* bn);

} // utils namespace
} // crypto namespace
} // ssu namespace
