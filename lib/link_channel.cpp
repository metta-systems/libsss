//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "link_channel.h"
#include "logging.h"

namespace ssu {

link_channel::~link_channel()
{
    unbind();
}

void link_channel::start(bool initiate)
{
    active_ = true;
}

void link_channel::stop()
{
    active_ = false;
}

channel_number link_channel::bind(link* link, const endpoint& remote_ep)
{
    assert(link);
    assert(!active_); // can't bind while channel is active
    assert(!link_);   // can't bind again if already bound

    // Find a free channel number for this remote endpoint.
    // Never assign channel zero - that's reserved for control packets.
    channel_number chan = 1;
    while (link->channel_for(remote_ep, chan) != nullptr) {
        if (++chan == 0)
            return 0;   // wraparound - no channels available
    }

    // Bind to this channel
    if (!bind(link, remote_ep, chan))
        return 0;

    return chan;
}

bool link_channel::bind(link* link, const endpoint& remote_ep, channel_number chan)
{
    assert(link);
    assert(!active_); // can't bind while channel is active
    assert(!link_);   // can't bind again if already bound

    if (link->channel_for(remote_ep, chan) != nullptr) {
        return false;
    }

    logger::debug() << "Bind " << remote_ep << " channel " << int(chan) << " to " << link;
    remote_ep_ = remote_ep;
    local_channel_number_ = chan;
    if (!link->bind_channel(remote_ep_, local_channel_number_, this)) {
        return false;
    }

    link_ = link;
    return true;
}

void link_channel::unbind()
{
    stop();
    if (link_) {
        link_->unbind_channel(remote_ep_, local_channel_number_);
        link_ = nullptr;
        local_channel_number_ = 0;
    }
}

int link_channel::may_transmit()
{
    assert(link_);
    return link_->may_transmit(remote_ep_);
}

} // ssu namespace
