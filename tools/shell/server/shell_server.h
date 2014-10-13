//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include "sss/server.h"
#include "shell_protocol.h"

class shell_server : public shell_protocol
{
private:
    sss::server srv;

    void got_connection();

public:
    shell_server(std::shared_ptr<sss::host> host);
};
