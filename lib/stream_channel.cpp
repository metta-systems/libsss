#include "stream_channel.h"
#include "logging.h"

namespace ssu {

/**
 * Maximum number of in-use SIDs to skip while trying to allocate one,
 * before we just give up and detach an existing one in this range.
 */
constexpr int max_sid_skip = 16;

stream_protocol::counter_t stream_channel::allocate_transmit_sid()
{
    counter_t sid = transmit_sid_counter_;
    if (transmit_sids_.find(sid) != transmit_sids_.end())
    {
        int maxsearch = 0x7ff0 - (transmit_sid_counter_ - transmit_sid_acked_);
        maxsearch = std::min(maxsearch, max_sid_skip);
        do {
            if (maxsearch-- <= 0) {
                logger::fatal() << "allocate_transmit_sid: no free SIDs";
                // @fixme: do the actual detach
            }
        } while (transmit_sids_.find(++sid) != transmit_sids_.end());
    }
    assert(sid >= transmit_sid_counter_);
    transmit_sid_counter_ = sid + 1;

    return sid;
}

} // ssu namespace
