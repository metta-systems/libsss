//

#include "arsenal/fusionary.hpp"
#include "close_frame.h"

//using namespace std;
namespace asio = boost::asio;
namespace mpl = boost::mpl;

// Write ACK frame.
int close_frame_t::write(asio::mutable_buffer output)
{
    write(output, hdr);
}

int close_frame_t::read(asio::const_buffer input)
{
	;
}

void close_frame_t::dispatch()
{
	;
}
