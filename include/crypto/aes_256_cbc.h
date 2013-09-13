#pragma once

#include <openssl/aes.h>
#include "byte_array.h"

namespace ssu {

class aes_256_cbc
{
    AES_KEY key_;

public:
    enum class type {
        encrypt,
        decrypt
    };

    aes_256_cbc(type which, byte_array const& key);

    // Encrypted data is padded to AES_BLOCK_SIZE.
    byte_array encrypt(byte_array const& in);
    // Decrypted data padding is not stripped.
    byte_array decrypt(byte_array const& in);
};

} // ssu namespace
