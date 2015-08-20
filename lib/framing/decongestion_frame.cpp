//

#include "arsenal/fusionary.hpp"
#include "decongestion_frame.h"

//using namespace std;
namespace asio = boost::asio;
namespace mpl = boost::mpl;


int decongestion_frame_t::write(asio::mutable_buffer output)
{
    write(output, hdr);
    write_buffer(output, data);
}

int decongestion_frame_t::read(asio::const_buffer input)
{
}

void decongestion_frame_t::dispatch()
{
	;
}
