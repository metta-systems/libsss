//

#include "arsenal/fusionary.hpp"
#include "ack_frame.h"

namespace asio = boost::asio;
namespace mpl = boost::mpl;

// Write ACK frame.
int ack_frame_t::write(asio::mutable_buffer output)
{
    write(output, header_);
    write_buffer(output, data_);
    return 1;
}

int ack_frame_t::read(asio::const_buffer input)
{
    return 1;
}

void ack_frame_t::dispatch(channel::ptr c)
{
}
