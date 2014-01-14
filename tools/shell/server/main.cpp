//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "settings_provider.h"
#include "shell_server.h"
#include "ssu/host.h"

int main(int argc, char **argv)
{
    auto settings = settings_provider::instance();

    // Initialize SST and read or create our host identity
    auto host(ssu::host::create(settings.get()));

    // Create and register the shell service
    shell_server svc(host);

    logger::info() << "mshd server listening with EID " << host->host_identity().id();

    host->run_io_service();
}
