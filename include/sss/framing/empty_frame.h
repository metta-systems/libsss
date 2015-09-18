#pragma once

#include "packet_frame.h"
#include "frame_format.h"

namespace sss { namespace framing {

using empty_frame_t = packet_frame_t<empty_frame_header>;

} }
