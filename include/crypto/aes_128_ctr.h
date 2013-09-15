//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <boost/array.hpp>
#include <openssl/aes.h>
#include "byte_array.h"

namespace ssu {

class aes_128_ctr
{
    AES_KEY key_;

public:
    aes_128_ctr(byte_array const& key);
    ~aes_128_ctr();

    byte_array encrypt(byte_array const& in, boost::array<uint8_t,AES_BLOCK_SIZE> iv);
    inline byte_array decrypt(byte_array const& in, boost::array<uint8_t,AES_BLOCK_SIZE> iv) {
        return encrypt(in, iv);
    }
};

} // ssu namespace
