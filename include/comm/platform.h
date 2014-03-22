//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <vector>
#include "comm/socket_endpoint.h"

namespace uia {
namespace comm {
namespace platform {

/**
 * Find all of the local host's IP addresses (platform-specific).
 */
std::vector<endpoint> local_endpoints();

} // platform namespace
} // comm namespace
} // uia namespace
