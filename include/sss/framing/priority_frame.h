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

namespace sss {
namespace framing {

class priority_frame_t : public packet_frame_t
{
public:
    int write(asio::mutable_buffer output) const;
    int read(asio::const_buffer input);
    void dispatch(channel::ptr);

private:
    sss::framing::priority_frame_header_t header_;
    string data_;
};
}
}
