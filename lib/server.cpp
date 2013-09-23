//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "server.h"
#include "stream.h"
#include "logging.h"

namespace ssu {

server::server(std::shared_ptr<host> h)
    : host_(h)
{
}

bool server::listen(std::string const& service_name, std::string const& service_desc,
                    std::string const& protocol_name, std::string const& protocol_desc)
{
    assert(!service_name.empty());
    assert(!service_desc.empty());
    assert(!protocol_name.empty());
    assert(!protocol_desc.empty());
    assert(!is_listening());

    logger::debug() << "Registering service '" << service_name << "' protocol '" << protocol_name << "'";

    // Make sure the stream_responder is initialized and listening.
    host_->instantiate_stream_responder();

    auto svcpair = make_pair(service_name, protocol_name);

    if (host_->is_listening(svcpair))
    {
        // error_string_ = boost::format("Service %$1$ with protocol %$2$ already registered.");
        return false;
    }

    service_name_ = service_name;
    service_description_ = service_desc;
    protocol_name_ = protocol_name;
    protocol_description_ = protocol_desc;

    host_->register_listener(svcpair, this);
    active_ = true;

    return true;
}

//todo: unique_ptr<stream>?
stream* server::accept()
{
    if (received_connections_.empty())
        return nullptr;
    base_stream* bs = received_connections_.front();
    received_connections_.pop();
    return new stream(bs);
}

} // ssu namespace
