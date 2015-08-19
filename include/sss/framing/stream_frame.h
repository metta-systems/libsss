#pragma once

#include "packet_frame.h"

namespace sss { namespace framing {

class stream_frame_t : public packet_frame_t
{
    stream_frame_header header_;
    string data_;
    // This is obviously a horrible API, rework it into something more sensible.
    int write(asio::mutable_buffer output,
               bool no_ack = false, bool init = false, bool end = false, bool usid = false);
    int read(asio::const_buffer input);
};

} }
