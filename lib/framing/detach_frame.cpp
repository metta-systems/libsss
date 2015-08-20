//

#include "arsenal/fusionary.hpp"
#include "detach_frame.h"

//using namespace std;
namespace asio = boost::asio;
namespace mpl = boost::mpl;

// Write Reset frame.
int detach_frame_t::write(asio::mutable_buffer output)
{
    write(output, header_);
    write_buffer(output, data_);
}

int detach_frame_t::read(asio::const_buffer input)
{
	//int read_size = 0;
	//fusionary::read()
	return 0; //read_size;
}

void detach_frame_t::dispatch(framing_t& framing)
{
	framing.find_stream_by_lsid(header_.lsid).detach();
}

