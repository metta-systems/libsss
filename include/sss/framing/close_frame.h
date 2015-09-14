#pragma once

#include "packet_frame.h"
#include "frame_format.h"

#include "sss/channels/channel.h"

namespace sss { namespace framing {

class close_frame_t : public packet_frame_t
{
public:
    int write(boost::asio::mutable_buffer output) const;
    int read(boost::asio::const_buffer input);
    void dispatch(channel::ptr);

    bool operator==(const close_frame_t& o)
    {
        return o.header_ == header_ && o.data_ == data_;
    }
    
private:
    sss::framing::close_frame_header header_;
    std::string data_;
};

} }
