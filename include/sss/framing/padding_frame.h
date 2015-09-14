#pragma once

#include "packet_frame.h"
#include "frame_format.h"

namespace sss { namespace framing {

class padding_frame_t : public packet_frame_t
{
public:
    int write(boost::asio::mutable_buffer output) const;
    int read(boost::asio::const_buffer input);

    bool operator==(const padding_frame_t& o)
    {
        return o.header_ == header_;
    }
    
private:
    sss::framing::padding_frame_header header_;
};

} }
