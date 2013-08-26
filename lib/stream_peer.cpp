#include "private/stream_peer.h"

namespace ssu {
namespace private {

stream_peer::stream_peer(std::shared_ptr<host>& host, const peer_id& remote_id)
    : host_(host)
    , remote_id_(remote_id)
{}

stream_peer::~stream_peer()
{}

void stream_peer::connect_channel()
{}

void stream_peer::initiate_key_exchange(link* l, const endpoint& ep)
{}

void stream_peer::channel_started(stream_channel* channel)
{}

void stream_peer::clear_primary_channel()
{}

void stream_peer::add_location_hint(const endpoint& hint)
{}

} // private namespace
} // ssu namespace
