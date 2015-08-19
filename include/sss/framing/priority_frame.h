#pragma once

#include "packet_frame.h"

namespace sss { namespace framing {

class priority_frame_t : public packet_frame_t
{
    int write(asio::mutable_buffer output,
               sss::framing::priority_frame_header_t hdr);

    int read(asio::const_buffer input);
};

} }
