//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "stream.h"
#include "stream_channel.h"
#include "stream_responder.h"
#include "private/stream_peer.h"
#include "base_stream.h"
#include "identity.h"
#include "logging.h"
#include "host.h"
#include "algorithm.h"

using namespace std;

namespace ssu {

//=================================================================================================
// stream
//=================================================================================================

stream::stream(shared_ptr<host> h)
    : host_(h)
{
}

stream::stream(base_stream* other_stream)
    : host_(other_stream->host_)
    , stream_(other_stream)
{
    assert(other_stream->owner_.lock() == nullptr);
    other_stream->owner_ = shared_from_this();
}

stream::~stream()
{
    disconnect();
    assert(stream_ == nullptr);
}

bool stream::connect_to(peer_id const& destination, 
    string service, string protocol,
    endpoint const& destination_endpoint_hint)
{
    // Determine a suitable target EID.
    // If the caller didn't specify one (doesn't know the target's EID),
    // then use the location hint as a surrogate peer identity.
    byte_array eid = destination.id();
    if (eid.is_empty()) {
        eid = identity::from_endpoint(destination_endpoint_hint).id().id();//UGH! :(
        assert(!eid.is_empty());
    }

    logger::debug() << "Connecting to peer with id " << eid;

    disconnect();

    stream_ = new base_stream(host_, eid, nullptr);

    // Start the actual network connection process
    stream_->connect_to(service, protocol);

    if (destination_endpoint_hint != endpoint())
        connect_at(destination_endpoint_hint);

    return true;
}

bool stream::add_location_hint(peer_id const& eid, endpoint const& hint)
{
    if (eid.is_empty()) {
        set_error("No target EID for location hint");
        return false;
    }

    host_->stream_peer(eid)->add_location_hint(hint);
    return true;
}

void stream::disconnect()
{
    if (stream_) {
        stream_->shutdown(shutdown_mode::close);
        delete stream_;
        stream_ = nullptr;
    }
}

void stream::shutdown(shutdown_mode mode)
{
    if (stream_) {
        stream_->shutdown(mode);
    }
}

bool stream::is_connected() const
{
    return stream_ != nullptr;
}

void stream::connect_at(endpoint const& ep)
{}

void stream::set_error(string const& error)
{
    // @todo: set error string
    error_notify(error);
}

ssize_t stream::write_data(const char* data, size_t size)
{
    if (!stream_) {
        set_error("Stream not connected");
        return -1;
    }
    return stream_->write_data(data, size, stream_protocol::flags::data_push);
}

//=================================================================================================
// stream_responder
//=================================================================================================

stream_responder::stream_responder(shared_ptr<host> host)
    : key_responder(host, magic_id)
{}

channel* stream_responder::create_channel(link_endpoint const& initiator_ep,
            byte_array const& initiator_eid,
            byte_array const& user_data_in, byte_array& user_data_out)
{
    stream_peer* peer = get_host()->stream_peer(initiator_eid);

    stream_channel* chan = new stream_channel(get_host(), peer, initiator_eid);
    if (!chan->bind(initiator_ep))
    {
        logger::warning() << "stream_responder - could not bind new channel";
        delete chan;
        return nullptr;
    }

    return chan;
}

//=================================================================================================
// Stream host state.
//=================================================================================================

stream_host_state::~stream_host_state()
{}

void stream_host_state::instantiate_stream_responder()
{
    if (!responder_)
        responder_ = new stream_responder(get_host());
    assert(responder_);
}

stream_peer* stream_host_state::stream_peer(peer_id const& id)
{
    if (!contains(peers_, id))
        peers_[id] = new class stream_peer(get_host(), id);
    return peers_[id];
}

class stream_peer* stream_host_state::stream_peer_if_exists(peer_id const& id)
{
    if (!contains(peers_, id))
        return nullptr;
    return peers_[id];
}

} // ssu namespace
