#pragma once

#include "packet_frame.h"
#include "frame_format.h"

namespace sss { namespace framing {

using detach_frame_t = packet_frame_t<detach_frame_header>;

} }
