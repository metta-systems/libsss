#include "ssu/host.h" // @todo Remove, temporarily used to make socket.h below compile
#include "ssu/socket_channel.h"
// when decoupled, should not need host.h include above
#include "comm/socket.h"

using namespace std;

namespace uia {
namespace comm {

//=================================================================================================
// socket
//=================================================================================================

std::string socket::status_string(socket::status s)
{
    switch (s) {
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

void
socket::set_active(bool active)
{
    active_ = active;
    if (active_) {
        host_->activate_socket(this);
    }
    else {
        host_->deactivate_socket(this);
    }
}

/**
 * Two packet types we could receive are stream packet (multiple types), starting with channel header
 * with non-zero channel number. It is handled by specific socket_channel.
 * Another type is negotiation packet which usually attempts to start a session negotiation, it should
 * have zero channel number. It is handled by registered socket_receiver (XXX rename to socket_responder?).
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
socket::receive(const byte_array& msg, const socket_endpoint& src)
{
    if (msg.size() < 4)
    {
        logger::debug() << "Ignoring too small UDP datagram";
        return;
    }

    logger::file_dump(msg, "received raw socket packet");

    // First byte should be a channel number.
    // Try to find an endpoint-specific channel.
    ssu::channel_number cn = msg.at(0);
    ssu::socket_channel* chan = channel_for(src, cn);
    if (chan) {
        return chan->receive(msg, src);
    }

    // If that doesn't work, it may be a global control packet:
    // if so, pass it to the appropriate socket_receiver.
    try {
        ssu::magic_t magic = msg.as<big_uint32_t>()[0];

        ssu::socket_receiver* recvr = host_->receiver(magic);
        if (recvr) {
            return recvr->receive(msg, src);
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
    }
}

bool
socket::bind_channel(endpoint const& ep, ssu::channel_number chan, ssu::socket_channel* lc)
{
    assert(channel_for(ep, chan) == nullptr);
    channels_.insert(make_pair(make_pair(ep, chan), lc));
    return true;
}

void
socket::unbind_channel(endpoint const& ep, ssu::channel_number chan)
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
