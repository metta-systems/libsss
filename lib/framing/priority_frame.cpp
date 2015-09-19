#include "arsenal/fusionary.hpp"
#include "priority_frame.h"

namespace asio = boost::asio;
namespace mpl = boost::mpl;

// Write Reset frame.
int priority_frame_t::write(asio::mutable_buffer output)
{
    write(output, header_);
    write_buffer(output, data_);
    return 1;
}

int priority_frame_t::read(asio::const_buffer input)
{
    return 1;
}

void priority_frame_t::dispatch(channel::ptr c)
{
}

