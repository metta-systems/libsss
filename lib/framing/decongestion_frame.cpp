#include "sss/framing/decongestion_frame.h"

#include "arsenal/fusionary.hpp"

namespace asio = boost::asio;
namespace mpl = boost::mpl;

namespace sss { namespace framing {

int decongestion_frame_t::write(asio::mutable_buffer output) const
{
    //write(output, hdr);
    //write_buffer(output, data);
    return 1;
}

int decongestion_frame_t::read(asio::const_buffer input)
{
    return 1;
}

void decongestion_frame_t::dispatch(channel::ptr)
{
}

} }
