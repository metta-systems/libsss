//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "shell_server.h"
#include "shell_session.h"

shell_server::shell_server(std::shared_ptr<ssu::host> host)
    : srv(host)
{
    srv.on_new_connection.connect([this]{got_connection();});

    if (!srv.listen(service_name, "Secure Remote Shell",
        protocol_name, "MettaNode Remote Shell Protocol"))
        logger::fatal() << "Can't register Shell service";
}

void shell_server::got_connection()
{
    logger::debug() << "Incoming shell server connection";
    while (auto stream = srv.accept())
    {
        new shell_session(stream);
    }
}
