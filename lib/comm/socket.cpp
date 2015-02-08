#include "arsenal/logging.h"
#include "arsenal/algorithm.h"
#include "arsenal/subrange.h"
#include "comm/socket.h"
#include "comm/socket_channel.h"
#include "comm/packet_receiver.h"

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
}

socket_channel::weak_ptr
socket::channel_for(string channel_key)
{
    if (!contains(channels_, channel_key)) {
        return socket_channel::ptr();
    }
    return channels_[channel_key];
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
 * Now the curvecp packets are impassable blobs of encrypted data.
 * The only magic we can use to differentiate is 8 byte header,
 * saying if this is Hello, Cookie, Initiate or Message packet.
 * Message packets also contain sender short-term public key and this
 * is what we use to demultiplex packets to channels.
 * Hello, Cookie and Initiate packets go to key exchange handler.
 * Adding new packet headers may allow additional handling via
 * host_interface.bind_receiver() function.
 */
void
socket::receive(byte_array const& msg, socket_endpoint const& src)
{
    if (msg.size() < 64) {
        return; // Ignore unrecognized packets.
    }

    logger::file_dump(msg, "received raw socket packet");

    //Proposed API: string_ref magic = msg.string_view(0, 8);
    string magic = msg.as_string().substr(0, 8); // @todo Optimize (use byte_array subrange)

    if (host_interface_->has_receiver_for(magic)) {
        // Forward this packet to key exchange handler.
        auto rcvr = host_interface_->receiver_for(magic).lock();
        if (rcvr) {
            return rcvr->receive(msg, src);
        }
    }

    if (magic == magic::message)
    {
        auto chan = channel_for(msg.as_string().substr(8, 32)); // @todo Optimize (use byte_array subrange)
        if (auto channel = chan.lock()) {
            return channel->receive(msg, src);
        }
    }

    // Ignore unrecognized packets.
}

bool
socket::bind_channel(string channel_key, socket_channel::weak_ptr lc)
{
    assert(channel_for(channel_key).lock() == nullptr);
    channels_[channel_key] = lc;
    return true;
}

void
socket::unbind_channel(string channel_key)
{
    channels_.erase(channel_key);
}

bool
socket::is_congestion_controlled(endpoint const&)
{
    return false;
}

size_t
socket::may_transmit(endpoint const&)
{
    logger::fatal() << "may_transmit() called on a non-congestion-controlled socket";
    return 0;
}

} // comm namespace
} // uia namespace
