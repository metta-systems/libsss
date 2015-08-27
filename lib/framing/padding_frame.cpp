//

#include "arsenal/fusionary.hpp"
#include "padding_frame.h"

namespace asio = boost::asio;
namespace mpl = boost::mpl;

// Write ACK frame.
int padding_frame_t::write(asio::mutable_buffer output)
{
    write(output, header_);
    return 1;
}

int padding_frame_t::read(asio::const_buffer input)
{
    read(input, header_);
    return 1;
}
