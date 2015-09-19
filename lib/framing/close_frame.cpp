#include "close_frame.h"

#include "arsenal/fusionary.hpp"

namespace asio = boost::asio;
namespace mpl = boost::mpl;

// Write ACK frame.
int close_frame_t::write(asio::mutable_buffer output)
{
    write(output, header_);
    return 1;
}

int close_frame_t::read(asio::const_buffer input)
{
    read(input, header_);
    return 1;
}

void close_frame_t::dispatch(channel::ptr c)
{
}
