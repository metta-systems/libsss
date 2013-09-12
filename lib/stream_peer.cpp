#include "private/stream_peer.h"
#include "stream_channel.h"

namespace ssu {
// namespace private_ {

const async::timer::duration_type stream_peer::connect_retry_period = boost::posix_time::minutes(1);

stream_peer::stream_peer(std::shared_ptr<host> const& host, peer_id const& remote_id)
    : host_(host)
    , remote_id_(remote_id)
{}

stream_peer::~stream_peer()
{}

void stream_peer::connect_channel()
{}

void stream_peer::initiate_key_exchange(link* l, const endpoint& ep)
{
    if (primary_channel_ and primary_channel_->link_status() == link::status::up)
        return;
}

void stream_peer::channel_started(stream_channel* channel)
{}

void stream_peer::clear_primary_channel()
{}

void stream_peer::add_location_hint(const endpoint& hint)
{}

// } // private_ namespace
} // ssu namespace
