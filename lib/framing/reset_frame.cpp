#include "sss/framing/reset_frame.h"

namespace sss {
namespace framing {

void
reset_frame_t::dispatch(channel_ptr c)
{
    // c->stream(header_.lsid).close();
}

} // framing namespace
} // sss namespace
