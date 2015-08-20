//

#include "arsenal/fusionary.hpp"
#include "priority_frame.h"

//using namespace std;
namespace asio = boost::asio;
namespace mpl = boost::mpl;

// Write Reset frame.
int priority_frame_t::write(asio::mutable_buffer output)
{
    write(output, hdr);
    write_buffer(output, data);
}

int priority_frame_t::read(asio::const_buffer input)
{
	;
}

void priority_frame_t::dispatch()
{
	;
}

