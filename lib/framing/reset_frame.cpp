//

#include "arsenal/fusionary.hpp"
#include "reset_frame.h"

namespace asio = boost::asio;
namespace mpl = boost::mpl;

// Write Reset frame.
int reset_frame_t::write(asio::mutable_buffer output)
{
    write(output, header_);
    write_buffer(output, data_);
    return 1;
}

int reset_frame_t::read(asio::const_buffer input)
{
    data_.reserve(header_.reason_phrase_length);
    fusionary::read(data_, input);
    return 1;
}

void reset_frame_t::dispatch(channel::ptr c)
{
    c->stream(header_.lsid).close();
}
