#pragma once

#include "packet_frame.h"

namespace sss { namespace framing {

class settings_frame_t : public packet_frame_t
{
public:
    int write(asio::mutable_buffer output) const;
    int read(asio::const_buffer input);
    void dispatch();
    
private:
	sss::framing::settings_frame_header_t header_;
	string data_;
};

} }
