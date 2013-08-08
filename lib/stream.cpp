//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "stream.h"
#include "abstract_stream.h"

namespace ssu {

stream::stream(std::shared_ptr<host>& h)
    : host_(h)
{
}

stream::stream(abstract_stream* other_stream)
    : stream_(other_stream)
    , host_(other_stream->host_)
{
    assert(other_stream->owner.lock() == nullptr);
    other_stream->owner = this;
}

stream::~stream()
{
    disconnect();
    assert(stream_ == nullptr);
}

bool stream::connect_to(peer_id& destination, 
    std::string service, std::string protocol,
    const endpoint& destination_endpoint_hint)
{
    return false;
}

void stream::disconnect()
{
}

bool stream::is_connected() const
{
    return stream_ != nullptr;
}

}
