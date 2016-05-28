//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "sss/internal/stream_peer.h"
#include "sss/channels/channel.h"
#include "arsenal/algorithm.h"
#include "routing/routing_client.h"
#include "routing/client_profile.h"
#include "sss/negotiation/channel_initiator.h"

using namespace std;
namespace ur = uia::routing;
using uia::comm::socket;
using namespace sss::negotiation;

namespace uia {
namespace routing {

routing_coordination::routing_coordination(shared_ptr<sss::host> host)
    : reconnect_timer_(host.get())
{
}
}
}

namespace sss {
namespace internal {

stream_peer::stream_peer(shared_ptr<host> const& host,
                         uia::peer_identity const& remote_id,
                         stream_peer::private_tag)
    : uia::internal::peer(host, remote_id)
    , coord_(host)
{
}

stream_peer::~stream_peer()
{
    BOOST_LOG_TRIVIAL(debug) << this << " ~stream_peer";
    // Clear the state of all streams associated with this peer.
    auto streams_copy = all_streams_;
    for (auto v : streams_copy) {
        v->clear();
    }
    assert(all_streams_.empty());
    assert(usid_streams_.empty());
}

void
stream_peer::routing_client_ready(ur::client* rc)
{
    if (contains(coord_.lookups_, rc))
        return; // Already polling this regserver

    // Make sure we're hooked up to this client's signals
    connect_routing_client(rc);

    // Start the lookup, with hole punching
    coord_.lookups_.insert(rc);
    rc->lookup(remote_id_, /*notify:*/ true);
}

void
stream_peer::connect_routing_client(ur::client* rc)
{
    if (contains(coord_.connected_routing_clients_, rc))
        return;
    coord_.connected_routing_clients_.insert(rc);

    // Listen for the lookup response
    rc->on_lookup_done.connect([this, rc](uia::peer_identity const& target_peer,
                                          uia::comm::endpoint const& peer_endpoint,
                                          ur::client_profile const& peer_profile) {
        lookup_done(rc, target_peer, peer_endpoint, peer_profile);
    });

    // Also make sure we hear if this regclient disappears
    rc->on_destroyed.connect([this](ur::client* rc) { regclient_destroyed(rc); });
}

void
stream_peer::lookup_done(ur::client* rc,
                         uia::peer_identity const& target_peer,
                         uia::comm::endpoint const& peer_endpoint,
                         ur::client_profile const& peer_profile)
{
    if (target_peer != remote_id_) {
        BOOST_LOG_TRIVIAL(debug) << "Got lookup_done for wrong id " << target_peer << " expecting "
                        << remote_id_ << " (harmless, ignored)";
        return; // ignore responses for other lookup requests
    }

    // Mark this outstanding lookup as completed.
    if (!contains(coord_.lookups_, rc)) {
        BOOST_LOG_TRIVIAL(debug) << "Stream peer - unexpected lookup_done signal";
        return; // ignore duplicates caused by concurrent requests
    }
    coord_.lookups_.erase(rc);

    // If the lookup failed, notify waiting streams as appropriate.
    if (peer_endpoint.address().is_unspecified()) {
        BOOST_LOG_TRIVIAL(debug) << "Lookup on " << target_peer << " failed";
        if (!coord_.lookups_.empty() or !key_exchanges_initiated_.empty())
            return; // There's still hope
        return on_channel_failed();
    }

    BOOST_LOG_TRIVIAL(debug) << "Stream peer - lookup found primary " << peer_endpoint
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

    add_location_hint(peer_endpoint); // temp
#if 0
    // Add the endpoint information we've received to our address list,
    // and initiate flow setup attempts to those endpoints.
    auto peer_endpoints = peer_profile.endpoints();
    peer_endpoints.push_back(peer_endpoint);

    // Final sorted list of endpoints to connect to.
    std::vector<endpoint> output_endpoints;

    // Filter and sort endpoints array.
    for (auto& ep : peer_endpoints)
    {
        BOOST_LOG_TRIVIAL(debug) << "Stream peer - secondary " << ep;
        // Ignore ep if it's a loopback.
        if (ep.address().is_loopback() or ep.address().is_unspecified()) {
            continue;
        }
        // Calculate common prefix length between ep and our local endpoints addresses.
        output_endpoints.push_back(ep);
    }

    for (auto& ep : output_endpoints
    {
        BOOST_LOG_TRIVIAL(debug) << "Stream peer - secondary " << ep;
        // Ignore ep if it's a loopback.
        if (ep.address().is_loopback() or ep.address().is_unspecified()) {
            continue;
        }

        // @todo Multiple key exchanges seem to cause some protocol confusion... fix it
        // add_location_hint(ep);
    }
#endif
}

void
stream_peer::regclient_destroyed(ur::client* rc)
{
    BOOST_LOG_TRIVIAL(debug) << "Stream peer - regclient destroyed before lookup done";

    coord_.lookups_.erase(rc);
    coord_.connected_routing_clients_.erase(rc);

    // If there are no more RegClients available at all,
    // notify waiting streams of connection failure
    // next time we get back to the main loop.
    if (no_lookups_possible()) {
        coord_.reconnect_timer_.start(boost::posix_time::milliseconds(0));
    }
}

} // internal namespace
} // sss namespace
