//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "ssu/private/stream_peer.h"
#include "ssu/stream_channel.h"
#include "arsenal/algorithm.h"
#include "routing/routing_client.h"
#include "routing/client_profile.h"

using namespace std;
namespace ur = uia::routing;

namespace ssu {
namespace internal {

constexpr int stream_peer::stall_warnings_max;

const async::timer::duration_type stream_peer::connect_retry_period = boost::posix_time::minutes(1);

stream_peer::stream_peer(shared_ptr<host> const& host,
                         peer_id const& remote_id,
                         stream_peer::private_tag)
    : host_(host)
    , remote_id_(remote_id)
    , reconnect_timer_(host.get())
{
    assert(!remote_id.is_empty());

    // If the remote_id is just an encapsulated IP endpoint,
    // then also use it as a destination address hint.
    identity ident(remote_id_.id());
    if (ident.is_ip_key_scheme()) {
        endpoint ep(ident.get_endpoint());
        if (ep.port() == 0) {
            ep.port(stream_protocol::default_port);
        }
        locations_.insert(ep);
    }

    reconnect_timer_.on_timeout.connect([this](bool failed){ retry_timeout(); });
}

stream_peer::~stream_peer()
{
    logger::debug() << this << " ~stream_peer";
    // Clear the state of all streams associated with this peer.
    auto streams_copy = all_streams_;
    for (auto v : streams_copy)
    {
        v->clear();
    }
    assert(all_streams_.empty());
    assert(usid_streams_.empty());
}

void stream_peer::connect_channel()
{
    assert(!remote_id_.is_empty());

    if (primary_channel_ and primary_channel_->link_status() == link::status::up)
        return; // Already have a working channel; don't need another yet.

    // @todo Need a way to determine if streams need to send. If no streams waiting to send
    // on this channel, don't even bother.
    //
    //if (receivers(SIGNAL(flowConnected())) == 0) return;

    logger::debug() << "Trying to connect channel with peer " << remote_id_;

    // @todo Ask routing to figure out other possible endpoints for this peer.

    // Send a lookup request to each known registration server.
    for (auto rc : host_->coordinator->routing_clients())
    {
        if (!rc->is_ready()) {
            // Can't poll an inactive regserver
            rc->on_ready.connect([this, rc] {
                routing_client_ready(rc);
            });
            continue;
        }

        routing_client_ready(rc);
    }

    // Initiate key exchange attempts to all already-known endpoints
    // using each of the network links we have available.
    for (auto link : host_->active_links())
    {
        for (auto endpoint : locations_)
        {
            initiate_key_exchange(link, endpoint);
        }
    }

    // Keep firing off connection attempts periodically
    reconnect_timer_.start(connect_retry_period);
}

void stream_peer::routing_client_ready(ur::client *rc)
{
    if (contains(lookups_, rc))
        return;   // Already polling this regserver

    // Make sure we're hooked up to this client's signals
    connect_routing_client(rc);

    // Start the lookup, with hole punching
    lookups_.insert(rc);
    rc->lookup(remote_id_, /*notify:*/true);
}

void stream_peer::connect_routing_client(ur::client *rc)
{
    if (contains(connected_routing_clients_, rc))
        return;
    connected_routing_clients_.insert(rc);

    // Listen for the lookup response
    rc->on_lookup_done.connect([this, rc](ssu::peer_id const& target_peer,
                                      ssu::endpoint const& peer_endpoint,
                                      ur::client_profile const& peer_profile)
    {
        lookup_done(rc, target_peer, peer_endpoint, peer_profile);
    });

    // Also make sure we hear if this regclient disappears
    rc->on_destroyed.connect([this](ur::client* rc) { regclient_destroyed(rc); });
}

#if 0
// Calculate affinity in bits of two IP addresses.
int
affinity(address const& a, address const& b)
{
    if (a.size() != b.size()) {
        return 0; // ipv4 and ipv6 do not match at all
    }
    for (int i = 0; i < a.size(); ++i)
    {
        int x = a.bytes()[i] ^ b.bytes()[i];
        if (x == 0) {
            continue;
        }
        // Byte difference detected - find first bit difference
        for (int j = 0; j < 8; ++j)
        {
            if (x & (0x80 >> j)) {
                return i*8 + j;
            }
        }
        // Should never be reached
        assert(0);
    }
    return a.size() * 8;  // addresses are identical
}
#endif

void
stream_peer::lookup_done(ur::client *rc, ssu::peer_id const& target_peer,
    ssu::endpoint const& peer_endpoint,
    ur::client_profile const& peer_profile)
{
    if (target_peer != remote_id_) {
        logger::debug() << "Got lookup_done for wrong id " << target_peer
            << " expecting " << remote_id_ << " (harmless, ignored)";
        return; // ignore responses for other lookup requests
    }

    // Mark this outstanding lookup as completed.
    if (!contains(lookups_, rc)) {
        logger::debug() << "Stream peer - unexpected lookup_done signal";
        return; // ignore duplicates caused by concurrent requests
    }
    lookups_.erase(rc);

    // If the lookup failed, notify waiting streams as appropriate.
    if (peer_endpoint.address().is_unspecified())
    {
        logger::debug() << "Lookup on " << target_peer << " failed";
        if (!lookups_.empty() or !key_exchanges_initiated_.empty())
            return;     // There's still hope
        return on_channel_failed();
    }

    logger::debug() << "Stream peer - lookup found primary " << peer_endpoint
        << ", num secondaries " << peer_profile.endpoints().size();

    // @todo
    // Find intersection between our and targets' IP addresses.
    // Prefer local network addresses first, initiate there in case the peer is in our network.

    // Sort hints by longest-common-IP-prefix.
    // This way the closest IP addresses would be at the top of the list.
    // Prefer local network addresses to external ones,
    // for the moment prefer ipv4 addresses over ipv6.

    // PROBLEM: A common shared external IP address will have ALL bits matching and therefore
    // longest-common prefix. Therefore match only internal endpoints.

#if 0
    // Need to distinguish internal and external endpoints in the endpoint list.
    for (ep : internal_endpoints)
    {
        int max_affinity = 0;
        for (peer_ep : peer_endpoints)
        {
            if (affinity(peer_ep, ep) > max_affinity)
            {
                // This peer_ep is better candidate for connection through ep.
                max_affinity = affinity(peer_ep, ep);
            }
        }
    }
#endif

    add_location_hint(peer_endpoint);//temp
#if 0
    // Add the endpoint information we've received to our address list,
    // and initiate flow setup attempts to those endpoints.
    auto peer_endpoints = peer_profile.endpoints();
    peer_endpoints.push_back(peer_endpoint);

    // Final sorted list of endpoints to connect to.
    std::vector<ssu::endpoint> output_endpoints;

    // Filter and sort endpoints array.
    for (auto& ep : peer_endpoints)
    {
        logger::debug() << "Stream peer - secondary " << ep;
        // Ignore ep if it's a loopback.
        if (ep.address().is_loopback() or ep.address().is_unspecified()) {
            continue;
        }
        // Calculate common prefix length between ep and our local endpoints addresses.
        output_endpoints.push_back(ep);
    }

    for (auto& ep : output_endpoints
    {
        logger::debug() << "Stream peer - secondary " << ep;
        // Ignore ep if it's a loopback.
        if (ep.address().is_loopback() or ep.address().is_unspecified()) {
            continue;
        }

        // @todo Multiple key exchanges seem to cause some protocol confusion... fix it
        // add_location_hint(ep);
    }
#endif
}

void stream_peer::regclient_destroyed(ur::client *rc)
{
    logger::debug() << "Stream peer - regclient destroyed before lookup done";

    lookups_.erase(rc);
    connected_routing_clients_.erase(rc);

    // If there are no more RegClients available at all,
    // notify waiting streams of connection failure
    // next time we get back to the main loop.
    if (no_lookups_possible()) {
        reconnect_timer_.start(boost::posix_time::milliseconds(0));
    }
}

void stream_peer::retry_timeout()
{
    // If we actually have an active flow now, do nothing.
    if (primary_channel_ and primary_channel_->link_status() == link::status::up)
        return;

    // Notify any waiting streams of failure
    if (no_lookups_possible()) {
        on_channel_failed();
    }

    // If there are (still) any waiting streams, fire off a new batch of connection attempts.
    connect_channel();
}

void stream_peer::initiate_key_exchange(link* l, const endpoint& ep)
{
    assert(ep != endpoint());

    // No need to initiate new channels if we already have a working one.
    if (primary_channel_ and primary_channel_->link_status() == link::status::up)
        return;

    // Don't simultaneously initiate multiple channels to the same endpoint.
    // @todo Eventually multipath support is needed.
    link_endpoint lep(l, ep);
    if (contains(key_exchanges_initiated_, lep))
    {
        logger::debug() << "Already attempting connection to " << ep;
        return;
    }

    logger::debug() << "Initiating key exchange from link " << l << " to remote endpoint " << ep;

    // Make sure our stream_responder exists to receive and dispatch incoming
    // key exchange control packets.
    host_->instantiate_stream_responder();

    // Create and bind a new channel.
    channel* chan = new stream_channel(host_, this, remote_id_);
    if (!chan->bind(l, ep)) {
        logger::warning() << "Could not bind new channel to target " << ep;
        delete chan;
        return on_channel_failed();
    }

    // Start the key exchange process for the channel.
    shared_ptr<negotiation::key_initiator> init =
        make_shared<negotiation::key_initiator>(chan, magic_id, remote_id_);

    init->on_completed.connect([this](std::shared_ptr<negotiation::key_initiator> ki, bool success)
    {
        completed(ki, success);
    });

    key_exchanges_initiated_.insert(make_pair(lep, init));
    init->exchange_keys();
}

void stream_peer::channel_started(stream_channel* channel)
{
    logger::debug() << "Stream peer - channel " << channel << " started";

    assert(channel->is_active());
    assert(channel->target_peer() == this);
    assert(channel->link_status() == link::status::up);

    if (primary_channel_)
    {
        // If we already have a working primary channel, we don't need a new one.
        if (primary_channel_->link_status() == link::status::up)
            return;

        // But if the current primary is on the blink, replace it.
        clear_primary_channel();
    }

    logger::debug() << "Stream peer - new primary channel " << channel;

    // Use this channel as our primary channel for this target.
    primary_channel_ = channel;
    stall_warnings_ = 0;

    // Watch the link status of our primary channel, so we can try to replace it if it fails.
    primary_channel_link_status_connection_ =
        primary_channel_->on_link_status_changed.connect([this](link::status new_status)
        {
            primary_status_changed(new_status);
        });

    // Notify all waiting streams
    on_channel_connected();
    on_link_status_changed(link::status::up);
}

void stream_peer::clear_primary_channel()
{
    if (!primary_channel_)
        return;

    auto old_primary = primary_channel_;
    primary_channel_ = nullptr;

    // Avoid getting further primary link status notifications from it
    primary_channel_link_status_connection_.disconnect();

    // Clear all transmit-attachments
    // and return outstanding packets to the streams they came from.
    old_primary->detach_all();
}

void stream_peer::add_location_hint(endpoint const& hint)
{
    assert(!remote_id_.is_empty());
    // assert(!hint.empty());

    if (contains(locations_, hint)) {
        return; // We already know; sit down...
    }

    logger::debug() << "Found endpoint " << hint << " for target " << remote_id_;

    // Add this endpoint to our set
    locations_.insert(hint);

    // Attempt a connection to this endpoint
    for (auto link : host_->active_links()) {
        initiate_key_exchange(link, hint);
    }
}

void stream_peer::completed(std::shared_ptr<negotiation::key_initiator> ki, bool success)
{
    assert(ki and ki->is_done());

    // Remove and schedule the key initiator for deletion, in case it wasn't removed already
    // (e.g., if key agreement failed).
    //
    // @todo Delete channel automatically if key_initiator failed...
    link_endpoint lep = ki->remote_endpoint();

    logger::debug() << "Stream peer key exchange for ID " << remote_id_ << " to " << lep
        << " completed " << (success ? "successfully" : "erroneously");

    assert(!contains(key_exchanges_initiated_, lep) or key_exchanges_initiated_[lep] == ki);
    key_exchanges_initiated_.erase(lep);

    ki->cancel();
    ki = nullptr;

    // If unsuccessful, notify waiting streams.
    if (!success)
    {
        if (no_lookups_possible()) {
            return on_channel_failed();
        }
        return; // There's still hope
    }

    // We should have an active primary channel at this point,
    // since stream_channel::start() attaches the channel if there isn't one.
    // Note: the reason we don't just set the primary right here
    // is because stream_channel::start() gets called on incoming streams too,
    // so servers don't have to initiate back-channels to their clients.

    // @todo This invariant doesn't hold here, fixme.
    // assert(primary_channel_ and primary_channel_->link_status() == link::status::up);
}

void stream_peer::primary_status_changed(link::status new_status)
{
    assert(primary_channel_);

    if (new_status == link::status::up)
    {
        stall_warnings_ = 0;
        // Now that we (again?) have a working primary channel, cancel and delete all
        // outstanding key_initiators that are still in an early enough stage not
        // to have possibly created receiver state.
        // (If we were to kill a non-early key_initiator, the receiver might pick one
        // of those streams as _its_ primary and be left with a dangling channel!)
        // For Multipath-SSU to work we rather should not destroy them here and set up
        // multiple channels at once.
        auto ki_copy = key_exchanges_initiated_;
        for (auto ki : ki_copy)
        {
            auto initiator = ki.second;
            if (!initiator->is_early()) {
                continue;   // too late - let it finish
            }
            logger::debug() << "Deleting " << initiator << " for " << remote_id_
                << " to " << initiator->remote_endpoint();

            assert(ki.first == initiator->remote_endpoint());
            key_exchanges_initiated_.erase(ki.first);
            initiator->cancel();
            initiator = nullptr;
        }

        return on_link_status_changed(new_status);
    }

    if (new_status == link::status::stalled)
    {
        if (++stall_warnings_ < stall_warnings_max)
        {
            logger::warning() << "Primary channel stall "
                << stall_warnings_ << " of " << stall_warnings_max;
            return on_link_status_changed(new_status);
        }
    }

    // Primary is at least stalled, perhaps permanently failed -
    // start looking for alternate paths right away for quick response.
    connect_channel();

    // Pass the signal on to all streams connected to this peer.
    on_link_status_changed(new_status);
}

} // internal namespace
} // ssu namespace
