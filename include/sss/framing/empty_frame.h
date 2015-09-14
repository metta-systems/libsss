#pragma once

#include "packet_frame.h"

#include <boost/asio/buffer.hpp>

namespace sss { namespace framing {

class empty_frame_t : public packet_frame_t
{
public:
    int write(boost::asio::mutable_buffer output) const;
    int read(boost::asio::const_buffer input);

    bool operator==(const empty_frame_t& o)
    {
        return true;
    }
};

} }
