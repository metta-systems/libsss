#pragma once

#include "packet_frame.h"

namespace sss { namespace framing {

class settings_frame_t : public packet_frame_t
{
    int write(asio::mutable_buffer output,
               sss::framing::settings_frame_header_t hdr, string data);

    int read(asio::const_buffer input);
};

} }
