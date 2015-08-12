//

#include "arsenal/fusionary.hpp"
#include "settings_frame.h"

namespace asio = boost::asio;
namespace mpl = boost::mpl;

// Write ACK frame.
int settings_frame_t::write(asio::mutable_buffer output)
{
    write(output, hdr);
    write_buffer(output, data);
    return 1;
}

int settings_frame_t::read(asio::const_buffer input)
{
    return 1;
}

void settings_frame_t::dispatch()
{
}
