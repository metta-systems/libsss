//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "stream.h"
#include "private/stream_peer.h"
#include "base_stream.h"
#include "identity.h"
#include "logging.h"
#include "host.h"

namespace ssu {

stream::stream(std::shared_ptr<host> h)
    : host_(h)
{
}

stream::stream(base_stream* other_stream)
    : stream_(other_stream)
    , host_(other_stream->host_)
{
    assert(other_stream->owner.lock() == nullptr);
    other_stream->owner = shared_from_this();
}

stream::~stream()
{
    disconnect();
    assert(stream_ == nullptr);
}

bool stream::connect_to(peer_id const& destination, 
    std::string service, std::string protocol,
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

void stream::set_error(const std::string& error)
{
    // @todo: set error string
    error_notify(error);
}

//=================================================================================================
// Stream host state.
//=================================================================================================

stream_host_state::~stream_host_state()
{}

stream_peer* stream_host_state::stream_peer(peer_id const& id)
{
    if (peers_.find(id) == peers_.end())
        peers_[id] = new class stream_peer(get_host(), id);
    return peers_[id];
}

class stream_peer* stream_host_state::stream_peer_if_exists(peer_id const& id)
{
    if (peers_.find(id) == peers_.end())
        return nullptr;
    return peers_[id];
}

} // ssu namespace
