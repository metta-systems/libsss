//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "sss/channels/channel.h"
#include "uia/comm/socket.h"

namespace sss {
/*
bool
socket_channel::bind(socket::weak_ptr socket, endpoint const& remote_ep, std::string channel_key)
{
    auto sock = socket.lock();
    assert(sock);
    assert(!is_active()); // can't bind while channel is active
    assert(!is_bound());  // can't bind again if already bound
    assert(channel_key.size() == 32);

    if (sock->channel_for(channel_key) != nullptr) {
        return false;
    }

    remote_ep_ = remote_ep;
    local_channel_key_ = channel_key;
    // @todo bind to message_receiver not socket...
    // this probably means we do not need socket_channel class and channel can do that, since
    // socket comms are abstracted via message_receiver
    if (!sock->bind_channel(local_channel_key_, shared_from_this())) {
        return false;
    }

    logger::debug() << "Bound local channel " << encode::to_base32x(channel_key) << " for " << remote_ep << " to " << sock;

    socket_ = socket;
    return true;
}
*/
void
socket_channel::unbind()
{
    stop();
    assert(!is_active());
    if (auto socket = socket_.lock())
    {
        // socket->unbind_channel(remote_ep_, local_channel_key_);
        socket.reset();//@fixme: resets only this instance, parent shared_ptr remains...
        local_channel_key_.clear();
    }
}

size_t socket_channel::may_transmit()
{
    if (auto socket = socket_.lock()) {
        return socket->may_transmit(remote_ep_);
    }
    return false;
}

} // sss namespace
