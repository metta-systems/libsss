#include "sss/framing/settings_frame.h"

#include "arsenal/fusionary.hpp"

namespace asio = boost::asio;
namespace mpl = boost::mpl;

namespace sss { namespace framing {

// Write ACK frame.
int settings_frame_t::write(asio::mutable_buffer output) const
{
    //write(output, header_);
    //write_buffer(output, data_);
    return 1;
}

int settings_frame_t::read(asio::const_buffer input)
{
    return 1;
}

void settings_frame_t::dispatch(channel::ptr c)
{
}

} }
