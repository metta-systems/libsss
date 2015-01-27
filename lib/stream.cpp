//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include <boost/range/adaptor/map.hpp>
#include <boost/range/algorithm/copy.hpp>
#include "arsenal/logging.h"
#include "arsenal/algorithm.h"
#include "sss/stream.h"
#include "sss/channels/stream_channel.h"
#include "sss/internal/stream_peer.h"
#include "sss/streams/base_stream.h"
#include "sss/channels/peer_identity.h"
#include "sss/host.h"

using namespace std;

namespace sss {

//=================================================================================================
// stream
//=================================================================================================

stream::stream(shared_ptr<host> h)
    : host_(h)
{
}

// @fixme Ignore parent for now...
stream::stream(shared_ptr<abstract_stream> other_stream, stream* parent)
    : host_(other_stream->host_)
    , stream_(other_stream)
{
    stream_ = other_stream;
    assert(stream_->owner_.lock() == nullptr);

    // @todo set stream i/o mode to read-writable and no buffering
}

shared_ptr<stream>
stream::create(shared_ptr<abstract_stream> other_stream)
{
    shared_ptr<stream> st = make_shared<stream>(other_stream);
    other_stream->owner_ = st;
    return st;
}

stream::~stream()
{
    disconnect();
    assert(stream_ == nullptr);
}

void stream::disconnect()
{
    if (!stream_) {
        return; // Already disconnected
    }

    // Disconnect our link status signal.
    // peer_link_status_changed_signal.disconnect();
    // stream_peer* peer = host_->stream_peer_if_exists(stream_->peerid_);
    // if (peer)
    //     peer->on_link_status_changed.disconnect(boost::bind(&stream::on_link_status_changed, this));

    // Clear the back-link from the base_stream.
    stream_->owner_.reset();

    // Start the graceful close process on the internal state.
    // With the back-link gone, the base_stream self-destructs when done.
    stream_->shutdown(shutdown_mode::close);

    // We're now officially closed.
    stream_ = nullptr;
    // setOpenMode(NotOpen);
}

void stream::shutdown(shutdown_mode mode)
{
    if (!stream_) return;

    stream_->shutdown(mode);

    // @todo Close this stream for reading or writing, or both.
    // Something like
    // if (mode & reset)
    //     setOpenMode(NotOpen);
    // if (mode & read)
    //     setOpenMode(isWritable() ? WriteOnly : NotOpen);
    // if (mode & write)
    //     setOpenMode(isReadable() ? ReadOnly : NotOpen);
}

bool stream::is_connected() const
{
    return stream_ != nullptr;
}

bool stream::is_link_up() const
{
    if (!stream_) return false;
    return stream_->is_link_up();
}

bool stream::connect_to(uia::peer_identity const& destination,
    string service, string protocol,
    uia::comm::endpoint const& destination_endpoint_hint)
{
    // Determine a suitable target EID.
    // If the caller didn't specify one (doesn't know the target's EID),
    // then use the location hint as a surrogate peer identity.
    byte_array eid = destination.id();
    // this won't work anymore @fixme
    if (eid.is_empty()) {
        eid = identity::from_endpoint(destination_endpoint_hint).id().id();//UGH! :(
        assert(!eid.is_empty());
    }

    disconnect();

    logger::debug() << "Connecting to peer with id " << eid;

    // Create a top-level application stream object for this connection.
    auto base = make_shared<base_stream>(host_, eid, nullptr);
    base->owner_ = shared_from_this();
    base->self_ = base; // Self-reference for the base_stream to stay around until finished.
    stream_ = base;

    // Get our link status signal hooked up, if it needs to be.
    connect_link_status_signal();

    // Start the actual network connection process
    base->connect_to(service, protocol);

    // We allow the client to start "sending" data immediately
    // even before the stream has fully connected.
    // setOpenMode(ReadWrite | Unbuffered);

    // If we were given a location hint, record it for setting up channels.
    if (destination_endpoint_hint != uia::comm::endpoint()) {
        connect_at(destination_endpoint_hint);
    }

    return true;
}

void stream::connect_link_status_signal()
{
    if (status_signal_connected_ or !stream_) {
        return;
    }

    internal::stream_peer* peer = host_->stream_peer(stream_->peerid_);

    peer->on_link_status_changed.connect([this](uia::comm::socket::status new_status) {
        on_link_status_changed(new_status);
    });
    status_signal_connected_ = true;
}

void stream::connect_at(uia::comm::endpoint const& ep)
{
    if (!stream_) {
        return;
    }
    host_->stream_peer(stream_->peerid_)->add_location_hint(ep);
}

bool stream::add_location_hint(uia::peer_identity const& eid, uia::comm::endpoint const& hint)
{
    if (eid.is_null())
    {
        set_error("No target EID for location hint");
        return false;
    }

    host_->stream_peer(eid)->add_location_hint(hint);
    return true;
}

void stream::set_priority(int priority)
{
    if (!stream_)
    {
        set_error("Stream not connected");
        return;
    }
    stream_->set_priority(priority);
}

int stream::current_priority() const
{
    if (!stream_) {
        return 0;
    }
    return stream_->current_priority();
}

void stream::set_error(string const& error)
{
    error_string_ = error;
    on_error_notify(error);
}

ssize_t stream::bytes_available() const
{
    if (!stream_) {
        return 0;
    }
    return stream_->bytes_available();
}

bool stream::at_end() const
{
    if (!stream_) {
        return true;
    }
    return stream_->at_end();
}

int stream::pending_records() const
{
    if (!stream_) {
        return 0;
    }
    return stream_->pending_records();
}

ssize_t stream::pending_record_size() const
{
    if (!stream_) {
        return 0;
    }
    return stream_->pending_record_size();
}

ssize_t stream::read_record(char* data, ssize_t max_size)
{
    if (!stream_)
    {
        set_error("Stream not connected");
        return -1;
    }
    return stream_->read_record(data, max_size);
}

byte_array stream::read_record(ssize_t max_size)
{
    if (!stream_)
    {
        set_error("Stream not connected");
        return byte_array();
    }
    return stream_->read_record(max_size);
}

ssize_t stream::read_data(char* data, ssize_t max_size)
{
    if (!stream_)
    {
        set_error("Stream not connected");
        return -1;
    }
    return stream_->read_data(data, max_size);
}

byte_array stream::read_data(ssize_t max_size)
{
    byte_array buf;
    max_size = min(max_size, bytes_available());
    buf.resize(max_size);
    ssize_t actual_size = read_data(buf.data(), buf.size());
    if (actual_size <= 0)
        return byte_array();
    if (actual_size < max_size)
        buf.resize(actual_size);
    return buf;
}

ssize_t stream::write_data(const char* data, ssize_t size)
{
    if (!stream_)
    {
        set_error("Stream not connected");
        return -1;
    }
    return stream_->write_data(data, size, stream_protocol::flags::data_push);
}

ssize_t stream::write_record(const char* data, ssize_t size)
{
    if (!stream_)
    {
        set_error("Stream not connected");
        return -1;
    }
    return stream_->write_data(data, size, stream_protocol::flags::data_record);
}

ssize_t stream::read_datagram(char* data, ssize_t max_size)
{
    if (!stream_)
    {
        set_error("Stream not connected");
        return -1;
    }
    return stream_->read_datagram(data, max_size);
}

byte_array stream::read_datagram(ssize_t max_size)
{
    if (!stream_)
    {
        set_error("Stream not connected");
        return byte_array();
    }
    return stream_->read_datagram(max_size);
}

ssize_t stream::write_datagram(const char* data, ssize_t size, datagram_type is_reliable)
{
    if (!stream_)
    {
        set_error("Stream not connected");
        return -1;
    }
    return stream_->write_datagram(data, size, is_reliable);
}

uia::peer_identity stream::local_host_id() const
{
    if (!stream_) {
        return uia::peer_identity();
    }
    return stream_->local_host_id();
}

uia::peer_identity stream::remote_host_id() const
{
    if (!stream_) {
        return uia::peer_identity();
    }
    return stream_->remote_host_id();
}

void stream::set_receive_buffer_size(ssize_t size)
{
    if (!stream_) {
        return;
    }
    stream_->set_receive_buffer_size(size);
}

void stream::set_child_receive_buffer_size(ssize_t size)
{
    if (!stream_) {
        return;
    }
    stream_->set_child_receive_buffer_size(size);
}

void stream::dump()
{
    if (!stream_)
    {
        logger::debug() << this << " is a detached user stream";
        return;
    }
    stream_->dump();
}

//-------------------------------------------------------------------------------------------------
// Substream management.
//-------------------------------------------------------------------------------------------------

shared_ptr<stream> stream::accept_substream()
{
    if (!stream_)
    {
        set_error("Stream not connected");
        return nullptr;
    }

    auto new_stream = stream_->accept_substream();
    if (!new_stream)
    {
        set_error("No waiting substreams");
        return nullptr;
    }

    return stream::create(new_stream);
}

shared_ptr<stream> stream::open_substream()
{
    if (!stream_)
    {
        set_error("Stream not connected");
        return nullptr;
    }

    auto new_stream = stream_->open_substream();
    if (!new_stream)
    {
        set_error("Unable to create substream"); // @todo Forward stream_'s error?
        return nullptr;
    }

    return stream::create(new_stream);
}

void stream::listen(listen_mode mode)
{
    if (!stream_)
    {
        set_error("Stream not connected");
        return;
    }
    stream_->listen(mode);
}

bool stream::is_listening() const
{
    if (!stream_) {
        return false;
    }
    return stream_->is_listening();
}

//=================================================================================================
// Stream host state.
//=================================================================================================

std::vector<std::shared_ptr<internal::stream_peer>>
stream_host_state::all_peers() const
{
    std::vector<std::shared_ptr<sss::internal::stream_peer>> values;
    boost::copy(peers_ | boost::adaptors::map_values, std::back_inserter(values));
    return values;
}

internal::stream_peer* stream_host_state::stream_peer(uia::peer_identity const& id)
{
    if (!contains(peers_, id))
    {
        peers_[id] = make_shared<internal::stream_peer>(get_host(), id,
            internal::stream_peer::private_tag{});
    }
    return peers_[id].get();
}

internal::stream_peer* stream_host_state::stream_peer_if_exists(uia::peer_identity const& id)
{
    if (!contains(peers_, id)) {
        return nullptr;
    }
    return peers_[id].get();
}

} // sss namespace
