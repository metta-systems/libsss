#include "arsenal/logging.h"
#include "arsenal/algorithm.h"
#include "arsenal/subrange.h"
#include "comm/socket.h"
#include "comm/socket_channel.h"
#include "comm/packet_receiver.h"

using namespace std;
using namespace boost;

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

void
socket::set_active(bool active)
{
    active_ = active;
    if (active_) {
        host_interface_->activate_socket(shared_from_this());
    }
    else {
        host_interface_->deactivate_socket(shared_from_this());
    }
}

constexpr size_t MIN_PACKET_SIZE = 64;

// @todo Use boost::string_ref or std::experimental::string_view for MOST of the stuff
// that handles refs into constantly allocated strings. Need to limit the scope somehow though.
string as_string(boost::asio::const_buffer value, size_t start, size_t size)
{
    return string(asio::buffer_cast<char const*>(value) + start, size);
}

/**
 * Now the curvecp packets are impassable blobs of encrypted data.
 * The only magic we can use to differentiate is 8 byte header,
 * saying if this is Hello, Cookie, Initiate or Message packet.
 * Hello, Cookie and Initiate packets go to key exchange handler.
 * Message packets go to message handler which forwards them to
 * appropriate channel based on source public key field.
 */
void
socket::receive(asio::const_buffer msg, socket_endpoint const& src)
{
    if (buffer_size(msg) >= MIN_PACKET_SIZE)
    {
        logger::file_dump(msg, "received raw socket packet");

        string magic = as_string(msg, 0, 8);

        if (auto rcvr = host_interface_->receiver_for(magic).lock()) {
            return rcvr->receive(msg, src); // @fixme Lose magic part?
        }
    }
    // Ignore too small or unrecognized packets.
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
