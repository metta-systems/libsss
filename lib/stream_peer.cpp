#include "private/stream_peer.h"
#include "stream_channel.h"

using namespace std;

namespace ssu {
// namespace private_ {

constexpr int stream_peer::stall_warnings_max;

const async::timer::duration_type stream_peer::connect_retry_period = boost::posix_time::minutes(1);

stream_peer::stream_peer(shared_ptr<host> const& host, peer_id const& remote_id)
    : host_(host)
    , remote_id_(remote_id)
{}

stream_peer::~stream_peer()
{}

void stream_peer::connect_channel()
{
    assert(!remote_id_.is_empty());

    if (primary_channel_ and primary_channel_->link_status() == link::status::up)
        return;

    // @todo: if no streams waiting to send on this channel, don't even bother...

    logger::debug() << "Trying to connect channel with peer " << remote_id_;

    // @todo Ask routing to figure out other possible endpoints for this peer.

    // Try to establish connection to all known endpoint addresses.
    for (auto link : host_->active_links())
    {
        for (auto endpoint : locations_)
        {
            initiate_key_exchange(link, endpoint);
        }
    }
}

void stream_peer::initiate_key_exchange(link* l, const endpoint& ep)
{
    if (primary_channel_ and primary_channel_->link_status() == link::status::up)
        return;

    // Don't simultaneously initiate multiple channels to the same endpoint. @todo
    link_endpoint lep(l, ep);
    if (key_exchanges_initiated_.find(lep) != key_exchanges_initiated_.end())
    {
        logger::debug() << "Already attempting connection to " << ep;
        return;
    }

    logger::debug() << "Initiating key exchange from link " << l << " to remote endpoint " << ep;

    host_->instantiate_stream_responder();

    channel* chan = new stream_channel(host_, this, remote_id_);
    if (!chan->bind(l, ep)) {
        logger::warning() << "Could not bind new channel to target " << ep;
        delete chan;
        return on_channel_failed();
    }

    // Start the key exchange process for the channel.
    shared_ptr<negotiation::key_initiator> init = make_shared<negotiation::key_initiator>(host_, chan, lep, magic_id, remote_id_);
    init->on_completed.connect(boost::bind(&stream_peer::completed, this, _1));
    key_exchanges_initiated_.insert(make_pair(lep, init));
    init->exchange_keys();
}

void stream_peer::channel_started(stream_channel* channel)
{}

void stream_peer::clear_primary_channel()
{
    // @todo channel cleanup
    primary_channel_ = nullptr;
}

void stream_peer::add_location_hint(const endpoint& hint)
{
    locations_.insert(hint);
}

void stream_peer::completed(bool success)
{
    logger::debug() << "Stream peer key exchange completed " << (success ? "successfully" : "erroneously");
}

// } // private_ namespace
} // ssu namespace
