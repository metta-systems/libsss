#include "abstract_stream.h"

namespace ssu {

abstract_stream::abstract_stream(std::shared_ptr<host> h)
    : host_(h)
{}

void abstract_stream::set_priority(int priority)
{}

int abstract_stream::priority() const
{
    return 0;
}

} // ssu namespace
