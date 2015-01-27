#include "arsenal/fusionary.hpp"

using namespace std;
namespace asio = boost::asio;
namespace mpl = boost::mpl;

// Write ACK frame.
void ack_frame::write(asio::mutable_buffer output, ack_frame_header_t hdr, string data)
{
    write(output, hdr);
    write_buffer(output, data);
}

void stream_frame::read(asio::const_buffer input)
{
}
