#pragma once

#include "packet_frame.h"

namespace sss {
namespace framing {

class framing_t;

class stream_frame_t : public packet_frame_t
{
public:
    int write(asio::mutable_buffer output) const;
    int read(asio::const_buffer input);
    void dispatch(channel::ptr);

private:
    stream_frame_header header_;
    string data_;
};
}
}
