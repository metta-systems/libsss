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

/**
 * Two packet types we could receive are stream packet (multiple types), starting with channel header
 * with non-zero channel number. It is handled by specific link_channel.
 * Another type is negotiation packet which usually attempts to start a session negotiation, it should
 * have zero channel number. It is handled by registered link_receiver (XXX rename to link_responder?).
 *
 *  Channel header (8 bytes)
 *   31          24 23                                0
 *  +--------------+-----------------------------------+
 *  |   Channel    |     Transmit Seq Number (TSN)     |
 *  +------+-------+-----------------------------------+
 *  | Rsvd | AckCt | Acknowledgement Seq Number (ASN)  |
 *  +------+-------+-----------------------------------+
 *        ... more channel-specific data here ...
 *
 *  Negotiation header (8+ bytes)
 *   31          24 23                                0
 *  +--------------+-----------------------------------+
 *  |  Channel=0   |     Negotiation Magic Bytes       |
 *  +--------------+-----------------------------------+
 *  |                  Chunk #1 size                   |
 *  +--------------------------------------------------+
 *  |                     .....                        |
 *  |                    Chunk #1                      |
 *  |                     .....                        |
 *  +--------------------------------------------------+
 *  |                  Chunk #2 size                   |
 *  +--------------------------------------------------+
 *  |                     .....                        |
 *  |                    Chunk #2                      |
 *  |                     .....                        |
 *  +--------------------------------------------------+
 *
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
        logger::file_dump dump(msg);
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

        link_receiver* recv = host_.receiver(magic);
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
    catch (std::exception& e)
    {
        logger::debug() << "Error deserializing received message: '" << e.what()
                        << "' buffer contents " << msg;
        return;
    }
}

bool
link::bind_channel(endpoint const& ep, channel_number chan, link_channel* lc)
{
    channels_.insert(std::make_pair(std::make_pair(ep, chan), lc));
    return true;
}

void
link::unbind_channel(endpoint const& ep, channel_number chan)
{
    channels_.erase(std::make_pair(ep, chan));
}

//=================================================================================================
// udp_link
//=================================================================================================

udp_link::udp_link(const endpoint& ep, link_host_state& h)
    : link(h)
    , udp_socket(h.get_io_service())
    , received_from(this, ep)
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

std::vector<endpoint>
udp_link::local_endpoints()
{
    return {udp_socket.local_endpoint()};
}

bool
udp_link::bind(endpoint const& ep)
{
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
    set_active(true);
    return true;
}

void
udp_link::unbind()
{
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
udp_link::udp_ready_read(const boost::system::error_code& error, std::size_t bytes_transferred)
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
