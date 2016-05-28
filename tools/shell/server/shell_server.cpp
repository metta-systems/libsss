//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "shell_server.h"
#include "shell_session.h"

shell_server::shell_server(std::shared_ptr<sss::host> host)
    : srv(host)
{
    srv.on_new_connection.connect([this]{got_connection();});

    if (!srv.listen(service_name, "Secure Remote Shell",
        protocol_name, "MettaNode Remote Shell Protocol"))
        BOOST_LOG_TRIVIAL(fatal) << "Can't register Shell service";
}

void shell_server::got_connection()
{
    BOOST_LOG_TRIVIAL(debug) << "Incoming shell server connection";
    while (auto stream = srv.accept())
    {
        new shell_session(stream);
    }
}
