#pragma once

#include "packet_frame.h"
#include "frame_format.h"

namespace sss { namespace framing {

using padding_frame_t = packet_frame_t<padding_frame_header>;

} }
