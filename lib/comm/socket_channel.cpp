//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "arsenal/logging.h"
#include "arsenal/base32x.h"
#include "comm/socket_channel.h"

namespace uia {
namespace comm {

bool
socket_channel::bind(socket::weak_ptr socket, endpoint const& remote_ep, std::string channel_key)
{
    assert(socket);
    assert(!is_active()); // can't bind while channel is active
    assert(!is_bound());  // can't bind again if already bound
    assert(channel_key.size() == 32);

    if (socket->channel_for(channel_key) != nullptr) {
        return false;
    }

    remote_ep_ = remote_ep;
    local_channel_key_ = channel_key;
    if (!socket->bind_channel(local_channel_key_, shared_from_this())) {
        return false;
    }

    logger::debug() << "Bound local channel " << encode::to_base32x(channel_key) << " for " << remote_ep << " to " << socket;

    socket_ = socket;
    return true;
}

void
socket_channel::unbind()
{
    stop();
    assert(!is_active());
    if (socket_)
    {
        socket_->unbind_channel(remote_ep_, local_channel_key_);
        socket_ = nullptr;
        local_channel_key_.clear();
    }
}

int
socket_channel::may_transmit()
{
    assert(socket_);
    return socket_->may_transmit(remote_ep_);
}

} // comm namespace
} // uia namespace
