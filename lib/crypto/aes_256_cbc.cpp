#include "crypto/aes_256_cbc.h"
#include "crypto.h"

namespace ssu {

aes_256_cbc::aes_256_cbc(type which, byte_array const& key)
{
    if (which == type::encrypt) {
        int keysize = key.size() * 8;
        assert(keysize == 128 or keysize == 192 or keysize == 256);
        int rc = AES_set_encrypt_key((const unsigned char*)key.const_data(), keysize, &key_);
        assert(rc == 0);
    } else {
        int keysize = key.size() * 8;
        assert(keysize == 128 or keysize == 192 or keysize == 256);
        int rc = AES_set_decrypt_key((const unsigned char*)key.const_data(), keysize, &key_);
        assert(rc == 0);
    }
}

byte_array aes_256_cbc::encrypt(byte_array const& in)
{
    int size = in.size();
    int padsize = (size + AES_BLOCK_SIZE-1) & ~(AES_BLOCK_SIZE-1);

    byte_array ivec;
    ivec.resize(AES_BLOCK_SIZE);
    crypto::fill_random(ivec.as_vector());

    byte_array out = ivec;
    out.resize(AES_BLOCK_SIZE + padsize);

    AES_cbc_encrypt((const unsigned char*)in.const_data(), (unsigned char*)out.data() + AES_BLOCK_SIZE,
        size, &key_, (unsigned char*)ivec.data(), AES_ENCRYPT);

    return out;
}

byte_array aes_256_cbc::decrypt(byte_array const& in)
{
    int padsize = in.size() - AES_BLOCK_SIZE;
    if (padsize <= 0 or (padsize % AES_BLOCK_SIZE) != 0)
        return byte_array();

    byte_array ivec = in;
    ivec.resize(AES_BLOCK_SIZE);//@fixme very inefficient

    byte_array out;
    out.resize(padsize);

    AES_cbc_encrypt((const unsigned char*)in.const_data() + AES_BLOCK_SIZE,
            (unsigned char*)out.data(),
            padsize, &key_, (unsigned char*)ivec.data(), AES_DECRYPT);

    return out;
}

} // ssu namespace
