//

#include "arsenal/fusionary.hpp"
#include "detach_frame.h"

namespace asio = boost::asio;
namespace mpl = boost::mpl;

// Write Reset frame.
int detach_frame_t::write(asio::mutable_buffer output)
{
    write(output, header_);
    write_buffer(output, data_);
    return 1;
}

int detach_frame_t::read(asio::const_buffer input)
{
    return 1;
}

void detach_frame_t::dispatch(channel::ptr c)
{
    c->find_stream(header_.lsid).detach();
}

