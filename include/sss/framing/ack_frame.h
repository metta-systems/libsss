#pragma once

#include "packet_frame.h"

namespace sss { namespace framing {

class ack_frame_t : public packet_frame_t
{
    int write(asio::mutable_buffer output,
               sss::framing::ack_frame_header_t hdr, string data);
    // data contains (possibly empty) array of 64 bit NACK entries
    int read(asio::const_buffer input);
};

} }
