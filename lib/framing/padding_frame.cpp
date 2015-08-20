//

#include "arsenal/fusionary.hpp"
#include "padding_frame.h"

//using namespace std;
namespace asio = boost::asio;
namespace mpl = boost::mpl;

// Write ACK frame.
int padding_frame_t::write(asio::mutable_buffer output)
{
    write(output, hdr);
    //write_buffer(output, data);
}

int padding_frame_t::read(asio::const_buffer input)
{
}
