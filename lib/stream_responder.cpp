//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "arsenal/logging.h"
#include "sss/host.h"
#include "uia/peer_identity.h"
#include "sss/channels/channel.h"
#include "sss/negotiation/kex_responder.h"
#include "sss/internal/stream_peer.h"
#include "sss/framing/framing_types.h"
#include "routing/routing_client.h"

using namespace std;
namespace ur = uia::routing;

namespace sss {

//=================================================================================================
// stream_responder
//=================================================================================================

/**
 * Private helper class, registers with socket layer to receive key exchange packets
 * and create new channels in response to initiate packets.
 * Only one instance ever created per host.
 */
class stream_responder : public negotiation::kex_responder, public stream_protocol
{
    /** @name Routing protocol */
    /**@{*/
    /// Set of routing clients we've connected to so far.
    unordered_set<ur::client*> connected_clients_;
    void connect_routing_client(ur::client* client);
    // Handlers:
    void created_client(ur::client* rc);
    void client_ready();
    void lookup_notify(uia::peer_identity const& target_peer,
                       uia::comm::endpoint const& peer_ep,
                       uia::routing::client_profile const& peer_profile);
    /**@}*/

    /** @name Key exchange protocol */
    /**@{*/
    channel_uptr create_channel(sodiumpp::secret_key local_short,
                                sodiumpp::public_key remote_short,
                                sodiumpp::public_key remote_long,
                                uia::comm::socket_endpoint const& initiator_ep) override;
    /**@}*/

public:
    stream_responder(host_ptr host);
};

stream_responder::stream_responder(shared_ptr<host> host)
    : kex_responder(host)
{
    logger::debug() << "Creating stream_responder " << this;

    // Get us connected to all currently extant routing clients
    for (ur::client* c : host->coordinator->routing_clients()) {
        connect_routing_client(c);
    }

    // Watch for newly created routing clients
    host->coordinator->on_routing_client_created.connect(
        [this](ur::client* c) { created_client(c); });
}

channel_uptr
stream_responder::create_channel(sodiumpp::secret_key local_short,
                                 sodiumpp::public_key remote_short,
                                 sodiumpp::public_key remote_long,
                                 uia::comm::socket_endpoint const& initiator_ep)
{
    internal::stream_peer* peer = get_host()->stream_peer(remote_long.get(), initiator_ep);

    unique_ptr<channel> chan =
        make_unique<stream_channel>(get_host(), peer, local_short, remote_short);

    return chan;
}

void
stream_responder::connect_routing_client(ur::client* c)
{
    logger::debug() << "Stream responder - connect routing client " << c->name();
    if (contains(connected_clients_, c)) {
        return;
    }

    connected_clients_.insert(c);
    c->on_ready.connect([this] { client_ready(); });
    c->on_lookup_notify.connect([this](uia::peer_identity const& target_peer,
                                       uia::comm::endpoint const& peer_ep,
                                       uia::routing::client_profile const& peer_profile) {
        lookup_notify(target_peer, peer_ep, peer_profile);
    });
}

void
stream_responder::created_client(ur::client* c)
{
    logger::debug() << "Stream responder - created client " << c->name();
    connect_routing_client(c);
}

void
stream_responder::client_ready()
{
    logger::debug() << "Stream responder - routing client ready";

    // Retry all outstanding lookups in case they might succeed now.
    for (auto peer : get_host()->all_peers()) {
        // peer->connect_channel();
    }
}

void
stream_responder::lookup_notify(uia::peer_identity const& target_peer,
                                uia::comm::endpoint const& peer_ep,
                                uia::routing::client_profile const& peer_profile)
{
    logger::debug() << "Stream responder - send r0 punch packet in response to lookup notify";
    // Someone at endpoint 'peer_ep' is apparently trying to reach us -
    // send them an R0 hole punching packet to their public endpoint.
    // @fixme perhaps make sure we might want to talk with them first?
    // e.g. check they're not in the blacklist.
    send_probe(peer_ep);
}

//=================================================================================================
// Stream host state.
//=================================================================================================

void
stream_host_state::instantiate_stream_responder()
{
}

} // sss namespace
