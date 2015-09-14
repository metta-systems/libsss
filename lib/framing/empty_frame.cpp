#include "sss/framing/empty_frame.h"

#include "arsenal/fusionary.hpp"

namespace asio = boost::asio;
namespace mpl = boost::mpl;

namespace sss { namespace framing {

// Write ACK frame.
int empty_frame_t::write(asio::mutable_buffer output) const
{
    //write(output, empty_frame_header);
    return 1;
}

int empty_frame_t::read(asio::const_buffer input)
{
    /*
    uint8_t h;
    read(input, h);
    if (h != empty_frame_header::value) {
        throw "Invalid empty frame header";
    }
    */
    return 1;
}

} }
