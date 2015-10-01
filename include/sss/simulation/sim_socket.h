//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include "uia/comm/socket.h"
#include "sss/forward_ptrs.h"

namespace sss {
namespace simulation {

class sim_socket : public uia::comm::socket, public std::enable_shared_from_this<sim_socket>
{
    simulator_ptr simulator_;
    sim_host_ptr host_;
    uint16_t port_{0};

public:
    sim_socket(sim_host_ptr host);
    ~sim_socket();

    bool bind(uia::comm::endpoint const& ep) override;
    void unbind() override;

    bool send(uia::comm::endpoint const& ep, const char* data, size_t size) override;

    std::vector<uia::comm::endpoint> local_endpoints() override;
    inline uint16_t local_port() override { return port_; }
    inline std::string error_string() override { return ""; }
    using uia::comm::socket::receive;
};

} // simulation namespace
} // sss namespace
