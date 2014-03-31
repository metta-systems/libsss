#include "arsenal/logging.h"
#include "arsenal/algorithm.h"
#include "comm/socket.h"
#include "comm/socket_channel.h"
#include "comm/socket_receiver.h"

using namespace std;

namespace uia {
namespace comm {

//=================================================================================================
// socket
//=================================================================================================

string socket::status_string(socket::status s)
{
    switch (s)
    {
        case status::down:    return "down";
        case status::stalled: return "stalled";
        case status::up:      return "up";
    }
}

socket::~socket()
{
    // Unbind all channels - @todo this should be automatic, use shared_ptr<socket_channel>s?
    for (auto v : channels_) {
        v.second->unbind();
    }
}

socket_channel*
socket::channel_for(endpoint const& src, channel_number cn)
{
    auto key = make_pair(src, cn);
    if (!contains(channels_, key))
        return nullptr;
    return channels_[key];
}

void
socket::set_active(bool active)
{
    active_ = active;
    if (active_) {
        host_interface_->activate_socket(this);
    }
    else {
        host_interface_->deactivate_socket(this);
    }
}

/**
 * Two packet types we could receive are stream packet (multiple types), starting with channel header
 * with non-zero channel number. It is handled by specific socket_channel.
 * Another type is negotiation packet which usually attempts to start a session negotiation, it should
 * have zero channel number. It is handled by registered socket_receiver.
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
socket::receive(byte_array const& msg, socket_endpoint const& src)
{
    if (msg.size() < 4)
    {
        logger::debug() << "Ignoring too small UDP datagram";
        return;
    }

    logger::file_dump(msg, "received raw socket packet");

    // First byte should be a channel number.
    // Try to find an endpoint-specific channel.
    channel_number cn = msg.at(0);
    socket_channel* chan = channel_for(src, cn);
    if (chan) {
        return chan->receive(msg, src);
    }

    if (cn) {
        logger::warning() << "No handler for channel number " << cn;
        return;
    }

    // Channel number zero must be a global control packet:
    // if so, pass it to the appropriate socket_receiver.
    try {
        magic_t magic = msg.as<big_uint32_t>()[0];

        socket_receiver* receiver = host_interface_->receiver_for(magic);
        if (receiver) {
            return receiver->receive(msg, src);
        }
        else
        {
            logger::debug() << "Received an invalid message, ignoring unknown receiver "
                            << hex(magic, 8, true) << " buffer contents " << msg;
        }
    }
    catch (exception& e)
    {
        logger::debug() << "Error deserializing received message: '" << e.what()
                        << "' buffer contents " << msg;
    }
}

bool
socket::bind_channel(endpoint const& ep, channel_number chan, socket_channel* lc)
{
    assert(channel_for(ep, chan) == nullptr);
    channels_.insert(make_pair(make_pair(ep, chan), lc));
    return true;
}

void
socket::unbind_channel(endpoint const& ep, channel_number chan)
{
    channels_.erase(make_pair(ep, chan));
}

bool
socket::is_congestion_controlled(endpoint const&)
{
    return false;
}

int
socket::may_transmit(endpoint const&)
{
    logger::fatal() << "may_transmit() called on a non-congestion-controlled socket";
    return -1;
}

} // comm namespace
} // uia namespace
