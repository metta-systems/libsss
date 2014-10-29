//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

namespace sss {
namespace channels {
namespace protocol {

// Probably not in the host_interface.h, rather somewhere in protocol.h
namespace magic {
const std::string hello    = "hellopkt";
const std::string cookie   = "cookipkt";
const std::string initiate = "init-pkt";
const std::string message  = "messagep";
} // magic namespace

} // protocol namespace
} // channels namespace
} // sss namespace

