#include "crypto/utils.h"
#include "byte_array.h"

namespace ssu {
namespace crypto {
namespace utils {

// Little helper functions for BIGNUM to byte_array conversions.

BIGNUM* ba2bn(byte_array const& ba)
{
    return BN_bin2bn((const unsigned char*)ba.data(), ba.size(), NULL);
}

byte_array bn2ba(BIGNUM const* bn)
{
    assert(bn != NULL);
    byte_array ba;
    ba.resize(BN_num_bytes(bn));
    BN_bn2bin(bn, (unsigned char*)ba.data());
    return ba;
}

} // utils namespace
} // crypto namespace
} // ssu namespace
