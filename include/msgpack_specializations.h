#include "msgpack.hpp"
#include "byte_array.h"
#include <boost/optional/optional.hpp>

namespace msgpack {

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

}  // namespace msgpack
