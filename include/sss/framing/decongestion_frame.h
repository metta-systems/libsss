#pragma once

#include "packet_frame.h"
#include "frame_format.h"

#include "sss/channels/channel.h"

namespace sss { namespace framing {

class decongestion_frame_t : public packet_frame_t<decongestion_frame_header>
{
public:
    void dispatch(channel::ptr);
};

} }
