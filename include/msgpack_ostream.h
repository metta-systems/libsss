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
 * Designed for use with the byte_array_owrap<msgpack_ostream>.
 */
class msgpack_ostream
{
    std::ostream& os_;

    // Implement this to support msgpack write buffer semantics.
    friend class msgpack::packer<msgpack_ostream>;
    void write(const char* data, size_t size) {
        os_.write(data, size);
    }

public:
    msgpack_ostream(std::ostream& os, unsigned int flags = 0)
        : os_(os)
    {}

    template <typename T>
    inline msgpack_ostream& operator << (const T& v)
    {
        msgpack::pack(os_, v);
        return *this;
    }
};

