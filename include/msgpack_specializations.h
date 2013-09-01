#pragma once

#include "msgpack.hpp"
#include "byte_array.h"
#include <boost/optional/optional.hpp>
#include <boost/array.hpp>

namespace msgpack {

// Specialization for enum types.
template <typename Stream, typename T>
inline packer<Stream>& operator << (packer<Stream>& o,
    typename std::enable_if<std::is_enum<T>::value, T>::type& v)
{
    o << to_underlying(v);
    return o;
}


// Specialization for byte_array.
template <typename Stream>
inline packer<Stream>& operator << (packer<Stream>& o, const byte_array& v)
{
    o.pack_array(v.size());
    o.pack_raw_body(v.data(), v.size());
    return o;
}

// Specialization for boost::optional<T>
template <typename Stream, typename T>
inline packer<Stream>& operator << (packer<Stream>& o, const boost::optional<T>& v)
{
    if (v.is_initialized()) {
        o.pack(*v);
    } else {
        o.pack_nil();
    }
    return o;
}

// Specialization for boost::array<T,N>
template <typename Stream, typename T, size_t N>
inline packer<Stream>& operator << (packer<Stream>& o, const boost::array<T,N>& v)
{
    o.pack_array(v.size());
    for(typename std::vector<T>::const_iterator it(v.begin()), it_end(v.end());
            it != it_end; ++it) {
        o.pack(*it);
    }
    return o;
}

// Specialization for boost::array<unsigned char,N>
template <typename Stream, size_t N>
inline packer<Stream>& operator << (packer<Stream>& o, const boost::array<unsigned char,N>& v)
{
    o.pack_array(v.size());
    o.pack_raw_body(v.data(), v.size());
    return o;
}

}  // namespace msgpack
