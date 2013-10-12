//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include "sign_key.h"
#include <openssl/rsa.h>

namespace ssu {
namespace crypto {

class rsa160_key : public sign_key
{
    RSA* rsa_;

    rsa160_key(RSA* rsa);

public:
    rsa160_key(byte_array const& key);
    rsa160_key(int bits = 0, unsigned e = 65537);
    ~rsa160_key();

    byte_array id() const override;

    byte_array public_key() const override;
    byte_array private_key() const override;

    byte_array sign(byte_array const& digest) const override;
    bool verify(byte_array const& digest, byte_array const& signature) const override;

private:
    void dump() const;
};

} // crypto namespace
} // ssu namespace
