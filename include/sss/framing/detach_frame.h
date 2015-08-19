#pragma once

#include "packet_frame.h"

class detach_frame_t : public packet_frame_t
{
    int write(asio::mutable_buffer output,
               sss::framing::detach_frame_header_t hdr);

    int read(asio::const_buffer input);
};
