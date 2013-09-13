//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "server.h"

namespace ssu {

server::server(std::shared_ptr<host> h)
    : host_(h)
{
}

bool server::listen(std::string const& service_name, std::string const& service_desc,
                    std::string const& protocol_name, std::string const& protocol_desc)
{
    host_->instantiate_stream_responder();
    return false;
}

bool server::is_listening() const
{
    return false;
}

//todo: unique_ptr<stream>?
stream* server::accept()
{
    return 0;
}

} // ssu namespace
