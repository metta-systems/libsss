//

#include "arsenal/fusionary.hpp"
#include "reset_frame.h"

//using namespace std;
namespace asio = boost::asio;
namespace mpl = boost::mpl;

// Write Reset frame.
int reset_frame_t::write(asio::mutable_buffer output)
{
    write(output, header_);
    write_buffer(output, data_);
}

int reset_frame_t::read(asio::const_buffer input)
{
	data_.reserve(header_.reason_phrase_length);
	fusionary::read(data_, input);
}

void reset_frame_t::dispatch(framing_t& framing)
{
	framing.find_stream_by_lsid(header_.lsid).close();
}
