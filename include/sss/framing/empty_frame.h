//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2015, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include "packet_frame.h"
#include "frame_format.h"

namespace sss {
namespace framing {

using empty_frame_t = packet_frame_t<empty_frame_header>;

} // framing namespace
} // sss namespace
