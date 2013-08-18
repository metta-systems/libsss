#include "identity.h"

namespace ssu {

identity identity::from_endpoint(endpoint const& ep)
{
    return identity();
}

byte_array identity::id() const
{
    return byte_array();
}

} // ssu namespace
