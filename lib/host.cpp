//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "host.h"

using namespace std;

namespace ssu {

shared_ptr<host>
host::create()
{
    shared_ptr<host> host(make_shared<host>());
    host->coordinator = make_shared<uia::routing::client_coordinator>(host); // @fixme LOOP
    return host;
}

shared_ptr<host>
host::create(settings_provider* settings, uint16_t default_port)
{
    shared_ptr<host> host(make_shared<host>());
    host->coordinator = make_shared<uia::routing::client_coordinator>(host); // @fixme LOOP
    host->init_link(settings, default_port);
    host->init_identity(settings);
    return host;
}

} // ssu namespace
