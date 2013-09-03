#pragma once

#include <vector>
#include "byte_array.h"
#include "msgpack.hpp"
#include "msgpack_specializations.h"

/**
 * MessagePack is a tagged format which is compact and rather efficient.
 * This file implements version v5 described at https://gist.github.com/frsyuki/5432559
 * It is byte-oriented and fits well for compact network serialization.
 */

/**
 * Act similar to boost.serialization archive but without the extra mumbo-jumbo.
 * Designed for use with the byte_array_iwrap<msgpack_istream>.
 */
class msgpack_istream
{
    std::istream& is_;

public:
    msgpack_istream(std::istream& is, unsigned int flags = 0)
        : is_(is)
    {}

    template <typename T>
    inline msgpack_istream& operator >> (T& v)
    {
        msgpack::unpacked obj;
        msgpack::unpack(is_, obj);
        obj.convert(&v);
        return *this;
    }
};

