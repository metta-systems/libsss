#pragma once

#include "packet_frame.h"

namespace sss { namespace framing {

class decongestion_frame_t : public packet_frame_t
{
    void write(asio::mutable_buffer output,
               sss::framing::decongestion_frame_header_t hdr);
    
    void read(asio::const_buffer input);
};

} }
