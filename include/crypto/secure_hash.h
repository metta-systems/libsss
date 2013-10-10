//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include "byte_array.h"

namespace ssu {
namespace crypto {

/**
 * Base class for secure hash functions implementations.
 */
class secure_hash
{
public:
    secure_hash();
    virtual ~secure_hash();

    virtual void init() = 0;
    virtual void update(char const* data, size_t size) = 0;
    virtual byte_array final() = 0;
};

} // crypto namespace
} // ssu namespace
