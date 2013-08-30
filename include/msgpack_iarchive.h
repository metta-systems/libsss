#include <cstddef> // std::size_t
#include <boost/archive/binary_iarchive.hpp>
#include "msgpack.hpp"

/**
 * MessagePack is a tagged format which is compact and rather efficient.
 * This file implements version v5 described at https://gist.github.com/frsyuki/5432559
 * It is byte-oriented and fits well for compact network serialization.
 */
class msgpack_iarchive
    : public boost::archive::binary_iarchive_impl<
        msgpack_iarchive,
        std::istream::char_type,
        std::istream::traits_type>
{
    typedef boost::archive::binary_iarchive_impl<
                msgpack_iarchive,
                std::istream::char_type,
                std::istream::traits_type> base_t;
    typedef boost::archive::basic_binary_iprimitive<
                msgpack_iarchive, 
                std::istream::char_type, 
                std::istream::traits_type> primitive_base_t;
    // give serialization implementation access to this class
    friend base_t;
    friend primitive_base_t;
    friend class boost::archive::basic_binary_iarchive<msgpack_iarchive>;
    friend class boost::archive::load_access;

    // default fall through for any types not specified here
    template<class T>
    void load(T & t){
        this->primitive_base_t::load(t);
    }

public:
    msgpack_iarchive(std::istream& is, unsigned int flags = 0) :
        base_t(is, flags | boost::archive::no_header)
    {
        this->base_t::init(flags);
    }
};

#include <boost/archive/impl/basic_binary_iarchive.ipp>
// #include <boost/archive/impl/archive_pointer_oserializer.ipp>
#include <boost/archive/impl/basic_binary_iprimitive.ipp>

// @todo #define BOOST_SERIALIZATION_REGISTER_ARCHIVE(Archive)

namespace boost {
namespace archive {

// explicitly instantiate for this type of binary stream
template class binary_iarchive_impl<
    msgpack_iarchive, 
    std::istream::char_type, 
    std::istream::traits_type
>;

} // archive namespace
} // boost namespace

#define BOOST_ARCHIVE_CUSTOM_IARCHIVE_TYPES msgpack_iarchive
