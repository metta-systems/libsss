#pragma once

#include "packet_frame.h"

namespace sss { namespace framing {

class reset_frame_t : public packet_frame_t
{
public:
    int write(asio::mutable_buffer output) const;
    int read(asio::const_buffer input);
    void dispatch();
    
private:
	sss::framing::reset_frame_header_t header_;
	string data_;
};

} }
