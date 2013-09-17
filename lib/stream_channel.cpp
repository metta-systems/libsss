#include <algorithm>
#include "stream_channel.h"
#include "logging.h"

namespace ssu {

/**
 * Maximum number of in-use SIDs to skip while trying to allocate one,
 * before we just give up and detach an existing one in this range.
 */
constexpr int max_sid_skip = 16;

stream_channel::stream_channel(std::shared_ptr<host> host, stream_peer* peer, const peer_id& id)
    : channel(host)
    , peer_(peer)
    , root_(std::make_shared<base_stream>(host, id, nullptr))
{
}

stream_channel::~stream_channel()
{}

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

void stream_channel::start(bool initiate)
{
    logger::debug() << "stream_channel: start " << (initiate ? "(initiator)" : "(responder)");
    super::start(initiate);
}

void stream_channel::stop()
{
    logger::debug() << "stream_channel: stop";
    super::stop();
}

bool stream_channel::transmit_ack(byte_array &pkt, uint64_t ackseq, unsigned ackct)
{
    logger::debug() << "stream_channel: transmit_ack";
    return false;
}

void stream_channel::acknowledged(uint64_t txseq, int npackets, uint64_t rxackseq)
{
    logger::debug() << "stream_channel: acknowledged";
}

void stream_channel::missed(uint64_t txseq, int npackets)
{
    logger::debug() << "stream_channel: missed";
}

void stream_channel::expire(uint64_t txseq, int npackets)
{
    logger::debug() << "stream_channel: expire";
}

bool stream_channel::channel_receive(uint64_t pktseq, byte_array &pkt)
{
    logger::debug() << "stream_channel: channel_receive";
    return false;
}

} // ssu namespace
