//

#include "arsenal/fusionary.hpp"
#include "settings_frame.h"

//using namespace std;
namespace asio = boost::asio;
namespace mpl = boost::mpl;

// Write ACK frame.
int settings_frame_t::write(asio::mutable_buffer output)
{
    write(output, hdr);
    write_buffer(output, data);
}

int settings_frame_t::read(asio::const_buffer input)
{
}

void settings_frame_t::dispatch()
{
	;
}
