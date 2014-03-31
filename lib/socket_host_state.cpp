//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "arsenal/logging.h"
#include "arsenal/flurry.h"
#include "arsenal/any_int_cast.h"
#include "arsenal/byte_array_wrap.h"
#include "arsenal/settings_provider.h"
#include "comm/platform.h"
#include "comm/socket.h"
#include "comm/socket_channel.h"
#include "comm/socket_receiver.h"
#include "comm/udp_socket.h"
#include "ssu/socket_host_state.h"

using namespace std;
using namespace boost::asio;

namespace ssu {

//=================================================================================================
// socket_host_state
//=================================================================================================

uia::comm::socket_receiver*
socket_host_state::receiver_for(uia::comm::magic_t magic)
{
    auto it = receivers_.find(magic);
    if (it == receivers_.end())
    {
        logger::debug() << "Receiver not found looking for magic " << hex(magic, 8, true);
        return 0;
    }
    return it->second;
}

shared_ptr<uia::comm::socket>
socket_host_state::create_socket()
{
    return make_shared<udp_socket>(get_host());
}

void
socket_host_state::init_socket(settings_provider* settings, uint16_t default_port)
{
    if (primary_socket_ and primary_socket_->is_active())
        return;

    if (primary_socket6_ and primary_socket6_->is_active())
        return;

    // See if a port number is recorded in our settings;
    // if so, use that instead of the specified default port.
    if (settings) {
        auto s_port = settings->get("port");
        if (!s_port.empty()) {
            int port = any_int_cast<int16_t>(s_port);
            if (port > 0 && port <= 65535) {
                default_port = port;
            }
        }
    }

    ip::udp::endpoint local_ep6(ip::address_v6::any(), default_port);
    ip::udp::endpoint local_ep(ip::address_v4::any(), default_port);

    // Create and bind the main sockets.
    primary_socket_ = create_socket();
    primary_socket6_ = create_socket();

    // See https://raw.github.com/boostcon/2011_presentations/master/wed/IPv6.pdf
    do {
        if (primary_socket_->bind(local_ep)) {
            break;
        }
        logger::warning() << "Can't bind to port " << dec << default_port << " ("
            << primary_socket_->error_string() << ") - trying another";

        local_ep.port(0);
        if (primary_socket_->bind(local_ep)) {
            break;
        }
        // @todo There might be a day when ipv4 does not exist anymore...
        logger::fatal() << "Couldn't bind the socket on ipv4 - " << primary_socket_->error_string();
    } while(0);

    do {
        if (primary_socket6_->bind(local_ep6)) {
            break;
        }
        logger::warning() << "Can't bind to port " << dec << default_port << " ("
            << primary_socket6_->error_string() << ") - trying another";

        local_ep6.port(0);
        if (primary_socket6_->bind(local_ep6)) {
            break;
        }
        logger::warning() << "Couldn't bind the socket on ipv6 ("
            << primary_socket6_->error_string() << "), trying ipv4";
    } while(0);

    default_port = primary_socket_->local_port();
    //ipv6 may have a different port here...
    // @todo Fix port to whatever worked for the first bind and fail if second bind fails?

    // Remember the port number we ended up using.
    if (settings) {
        settings->set("port", static_cast<int64_t>(default_port));
    }
}

unordered_set<uia::comm::endpoint>
socket_host_state::active_local_endpoints()
{
    unordered_set<uia::comm::endpoint> result;
    for (auto s : active_sockets())
    {
        assert(s->is_active());
        for (auto ep : s->local_endpoints()) {
            result.insert(ep);
        }
    }
    return result;
}

} // ssu namespace
