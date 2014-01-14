//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "ssu/protocol.h"

namespace ssu {
constexpr size_t stream_protocol::mtu;
constexpr size_t stream_protocol::min_receive_buffer_size;
constexpr magic_t stream_protocol::magic_id;
constexpr int stream_protocol::max_service_record_size;
}
