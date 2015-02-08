//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "sss/host.h"

using namespace std;

namespace sss {

host::ptr host::create()
{
    shared_ptr<host> host(make_shared<host>(private_tag()));
    host->coordinator = make_shared<uia::routing::client_coordinator>(host); // @fixme ptr LOOP
    return host;
}

host::ptr host::create(settings_provider* settings, uint16_t default_port)
{
    shared_ptr<host> host(make_shared<host>(private_tag()));
    host->coordinator = make_shared<uia::routing::client_coordinator>(host); // @fixme ptr LOOP
    // coordinator should have a weak_ptr to host here...
    host->init_socket(settings, default_port);
    host->init_identity(settings);
    return host;
}

} // sss namespace
