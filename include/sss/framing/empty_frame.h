#pragma once

#include "packet_frame.h"

namespace sss { namespace framing {

class empty_frame_t : public packet_frame_t
{
public:
    int write(asio::mutable_buffer output) const;
    int read(asio::const_buffer input);
    
private:
	sss::framing::padding_frame_header_t header_;
	// No data
};

} }
