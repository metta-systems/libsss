#include "empty_frame.h"

#include "arsenal/fusionary.hpp"

namespace asio = boost::asio;
namespace mpl = boost::mpl;

// Write ACK frame.
int empty_frame::write(asio::mutable_buffer output)
{
    write(output, empty_frame_header_t);
    return 1;
}

int empty_frame::read(asio::const_buffer input)
{
    uint8_t h;
    read(input, h);
    if (h != empty_frame_header_t::value) {
        throw "Invalid empty frame header";
    }
    return 1;
}

