#include <cstddef> // std::size_t
// #include <boost/archive/detail/common_oarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include "msgpack.h"

class msgpack_oarchive
    : public boost::archive::binary_oarchive_impl<
        msgpack_oarchive,
        std::ostream::char_type,
        std::ostream::traits_type>
{
    typedef boost::archive::binary_oarchive_impl<msgpack_oarchive, std::ostream::char_type, std::ostream::traits_type> base;
    // give serialization implementation access to this clas
    friend class boost::archive::detail::interface_oarchive<msgpack_oarchive>;
    friend class boost::archive::basic_binary_oarchive<msgpack_oarchive>;
    friend class boost::archive::save_access;

    void save_override(const bool t, int)
    {
        msgpack::encode_boolean(*this, t);
    }

    void save_override(byte_array const& t, int)
    {
        msgpack::encode_array(*this, t, 0xffffffff);
    }

public:
    msgpack_oarchive(std::ostream & os, unsigned int flags = 0) :
        boost::archive::binary_oarchive_impl<msgpack_oarchive,
            std::ostream::char_type,
            std::ostream::traits_type>
        (
            os, 
            flags | boost::archive::no_header
        )
    {}

    // msgpack_oarchive(byte_array& buffer);
    // how to make this work without loads of code? we need to instantiate two extra classes...
};

// @todo #define BOOST_SERIALIZATION_REGISTER_ARCHIVE(Archive)
