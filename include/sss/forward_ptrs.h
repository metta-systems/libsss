//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2015, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <memory>

namespace uia {
namespace comm {

class socket;
class socket_channel;
class packet_receiver;

using socket_ptr  = std::shared_ptr<socket>;
using socket_wptr = std::weak_ptr<socket>;
using socket_uptr = std::unique_ptr<socket>;

using socket_channel_ptr  = std::shared_ptr<socket_channel>;
using socket_channel_wptr = std::weak_ptr<socket_channel>;
using socket_channel_uptr = std::unique_ptr<socket_channel>;

using packet_receiver_ptr  = std::shared_ptr<packet_receiver>;
using packet_receiver_wptr = std::weak_ptr<packet_receiver>;
using packet_receiver_uptr = std::unique_ptr<packet_receiver>;

} // comm namespace
} // uia namespace

namespace sss {

class host;
class channel;
class stream_channel;
class base_stream;
class abstract_stream;

using host_ptr  = std::shared_ptr<host>;
using host_wptr = std::weak_ptr<host>;
using host_uptr = std::unique_ptr<host>;

using channel_ptr  = std::shared_ptr<channel>;
using channel_wptr = std::weak_ptr<channel>;
using channel_uptr = std::unique_ptr<channel>;

using stream_channel_ptr  = std::shared_ptr<stream_channel>;
using stream_channel_wptr = std::weak_ptr<stream_channel>;
using stream_channel_uptr = std::unique_ptr<stream_channel>;

using base_stream_ptr  = std::shared_ptr<base_stream>;
using base_stream_wptr = std::weak_ptr<base_stream>;
using base_stream_uptr = std::unique_ptr<base_stream>;

using abstract_stream_ptr  = std::shared_ptr<abstract_stream>;
using abstract_stream_wptr = std::weak_ptr<abstract_stream>;
using abstract_stream_uptr = std::unique_ptr<abstract_stream>;

namespace negotiation {

class kex_initiator;

using kex_initiator_ptr  = std::shared_ptr<kex_initiator>;
using kex_initiator_wptr = std::weak_ptr<kex_initiator>;

} // negotiation namespace

namespace simulation {

class sim_host;
class sim_connection;
class simulator;

using sim_host_ptr  = std::shared_ptr<sim_host>;
using sim_host_wptr = std::weak_ptr<sim_host>;
using sim_host_uptr = std::unique_ptr<sim_host>;

using sim_connection_ptr  = std::shared_ptr<sim_connection>;
using sim_connection_wptr = std::weak_ptr<sim_connection>;
using sim_connection_uptr = std::unique_ptr<sim_connection>;

using simulator_ptr  = std::shared_ptr<simulator>;
using simulator_wptr = std::weak_ptr<simulator>;
using simulator_uptr = std::unique_ptr<simulator>;

} // simulation namespace
} // sss namespace
