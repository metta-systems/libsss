//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include "server.h"
#include "shell_protocol.h"

class shell_server : public shell_protocol
{
private:
    ssu::server srv;

    void got_connection();

public:
    shell_server(std::shared_ptr<ssu::host> host);
};
