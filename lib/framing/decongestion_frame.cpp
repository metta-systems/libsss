//

#include "arsenal/fusionary.hpp"
#include "decongestion_frame.h"

namespace asio = boost::asio;
namespace mpl = boost::mpl;


int decongestion_frame_t::write(asio::mutable_buffer output)
{
    write(output, hdr);
    write_buffer(output, data);
    return 1;
}

int decongestion_frame_t::read(asio::const_buffer input)
{
    return 1;
}

void decongestion_frame_t::dispatch()
{
}
