//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "link.h"
#include "link_channel.h"
#include "link_receiver.h"
#include "logging.h"
#include "flurry.h"
#include "byte_array_wrap.h"
#include "settings_provider.h"
#include "host.h"

using namespace std;

namespace ssu {

//=================================================================================================
// link_receiver
//=================================================================================================

void link_receiver::bind()
{
    logger::debug() << "Link receiver " << this << " binds for magic " << hex(magic_, 8, true);
    host_.bind_receiver(magic_, this);
}

void link_receiver::unbind()
{
    logger::debug() << "Link receiver " << this << " unbinds magic " << hex(magic_, 8, true);
    host_.unbind_receiver(magic_);
}

//=================================================================================================
// link_host_state
//=================================================================================================

link_receiver*
link_host_state::receiver(magic_t magic)
{
    auto it = receivers_.find(magic);
    if (it == receivers_.end())
    {
        logger::debug() << "Receiver not found looking for magic " << hex(magic, 8, true);
        return 0;
    }
    return it->second;
}

shared_ptr<link>
link_host_state::create_link()
{
    return make_shared<udp_link>(get_host()); // @fixme
}

void
link_host_state::init_link(settings_provider* settings, uint16_t default_port)
{
    if (primary_link_ and primary_link_->is_active())
        return;

    // @fixme not ipv6-ready!!
    // This binds only to ipv4 local address.
    boost::asio::ip::udp::endpoint local_ep(boost::asio::ip::address_v4::any(), default_port);

    primary_link_ = create_link();
    if (!primary_link_->bind(local_ep))
    {
        local_ep.port(0);
        if (!primary_link_->bind(local_ep))
        {
            logger::fatal() << "Couldn't bind the link";
        }
    }
}

//=================================================================================================
// link_endpoint
//=================================================================================================

bool
link_endpoint::send(const char *data, int size) const
{
    if (auto l = link_/*.lock()*/)
    {
        return l->send(*this, data, size);
    }
    logger::debug() << "Trying to send on a nonexistent socket";
    return false;
}

//=================================================================================================
// link
//=================================================================================================

link::~link()
{
}

void
link::set_active(bool active)
{
    active_ = active;
    if (active_) {
        host_->activate_link(this);
    }
    else {
        host_->deactivate_link(this);
    }
}

/**
 * Two packet types we could receive are stream packet (multiple types), starting with channel header
 * with non-zero channel number. It is handled by specific link_channel.
 * Another type is negotiation packet which usually attempts to start a session negotiation, it should
 * have zero channel number. It is handled by registered link_receiver (XXX rename to link_responder?).
 *
 *  Channel header (8 bytes)
 *   31          24 23                                0
 *  +--------------+-----------------------------------+
 *  |   Channel    |     Transmit Seq Number (TSN)     | 4 bytes
 *  +------+-------+-----------------------------------+
 *  | Rsvd | AckCt | Acknowledgement Seq Number (ASN)  | 4 bytes
 *  +------+-------+-----------------------------------+
 *        ... more channel-specific data here ...        variable length
 *
 *  Negotiation header (8+ bytes)
 *   31          24 23                                0
 *  +--------------+-----------------------------------+
 *  |  Channel=0   |     Negotiation Magic Bytes       | 4 bytes
 *  +--------------+-----------------------------------+
 *  |               Flurry array of chunks             | variable length
 *  +--------------------------------------------------+
 */
void
link::receive(const byte_array& msg, const link_endpoint& src)
{
    if (msg.size() < 4)
    {
        logger::debug() << "Ignoring too small UDP datagram";
        return;
    }

    {
        logger::file_dump(msg, "received_raw.bin");
        logger::file_dump(msg, "dump.bin");
    }

    // First byte should be a channel number.
    // Try to find an endpoint-specific channel.
    channel_number cn = msg.at(0);
    link_channel* chan = channel_for(src, cn);
    if (chan)
    {
        return chan->receive(msg, src);
    }

    try {
        big_uint32_t magic = msg.as<big_uint32_t>()[0];

        link_receiver* recv = host_->receiver(magic);
        if (recv)
        {
            return recv->receive(msg, src);
        }
        else
        {
            logger::debug() << "Received an invalid message, ignoring unknown channel/receiver "
                            << hex(magic, 8, true) << " buffer contents " << msg;
        }
    }
    catch (exception& e)
    {
        logger::debug() << "Error deserializing received message: '" << e.what()
                        << "' buffer contents " << msg;
        return;
    }
}

bool
link::bind_channel(endpoint const& ep, channel_number chan, link_channel* lc)
{
    channels_.insert(make_pair(make_pair(ep, chan), lc));
    return true;
}

void
link::unbind_channel(endpoint const& ep, channel_number chan)
{
    channels_.erase(make_pair(ep, chan));
}

bool
link::is_congestion_controlled(endpoint const&)
{
    return false;
}

int
link::may_transmit(endpoint const&)
{
    logger::fatal() << "may_transmit() called on non-congestion-controlled link";
    return -1;
}

//=================================================================================================
// udp_link
//=================================================================================================

udp_link::udp_link(shared_ptr<host> host)
    : link(host)
    , udp_socket(host->get_io_service())
    , received_from(this, endpoint()) // @fixme Dummy endpoint initializer here... init in bind()?
{}

void
udp_link::prepare_async_receive()
{
    boost::asio::streambuf::mutable_buffers_type buffer = received_buffer.prepare(2048);
    udp_socket.async_receive_from(
        boost::asio::buffer(buffer),
        received_from,
        boost::bind(&udp_link::udp_ready_read, this,
          boost::asio::placeholders::error,
          boost::asio::placeholders::bytes_transferred));
}

vector<endpoint>
udp_link::local_endpoints()
{
    return {udp_socket.local_endpoint()};
}

bool
udp_link::bind(endpoint const& ep)
{
    logger::debug() << "udp_link bind on endpoint " << ep;
    boost::system::error_code ec;
    udp_socket.open(ep.protocol(), ec);
    if (ec) {
        logger::warning() << ec;
        return false;
    }
    udp_socket.bind(ep, ec);
    if (ec) {
        logger::warning() << ec;
        return false;
    }
    // once bound, can start receiving datagrams.
    prepare_async_receive();
    logger::debug() << "Bound udp_link on " << ep;
    set_active(true);
    return true;
}

void
udp_link::unbind()
{
    logger::debug() << "udp_link unbind";
    udp_socket.shutdown(boost::asio::ip::udp::socket::shutdown_both);
    udp_socket.close();
    set_active(false);
}

bool
udp_link::send(const endpoint& ep, const char *data, size_t size)
{
    return udp_socket.send_to(boost::asio::buffer(data, size), ep);
}

void
udp_link::udp_ready_read(const boost::system::error_code& error, size_t bytes_transferred)
{
    if (!error)
    {
        logger::debug() << "Received " << bytes_transferred << " bytes via UDP link";
        byte_array b(boost::asio::buffer_cast<const char*>(received_buffer.data()), bytes_transferred);
        receive(b, received_from);
        received_buffer.consume(bytes_transferred);
        prepare_async_receive();
    }
}

}
