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
#include "sss/forward_ptrs.h"

namespace sss {
namespace framing {

class ack_frame_t : public packet_frame_t<ack_frame_header>
{
public:
    void dispatch(channel_ptr);
};

} // framing namespace
} // sss namespace
