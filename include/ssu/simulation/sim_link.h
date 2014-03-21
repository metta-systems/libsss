//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include "ssu/host.h" // @todo Remove, temporarily used to make socket.h below compile
// when decoupled, should not need host.h include above
#include "comm/socket.h"

namespace ssu {

class host;

namespace simulation {

class sim_host;
class simulator;

class sim_link : public uia::comm::socket, public std::enable_shared_from_this<sim_link>
{
    std::shared_ptr<simulator> simulator_;
    std::shared_ptr<sim_host> host_;
    uint16_t port_{0};

public:
    sim_link(std::shared_ptr<sim_host> host);
    ~sim_link();

    bool bind(uia::comm::endpoint const& ep) override;
    void unbind() override;

    bool send(uia::comm::endpoint const& ep, const char *data, size_t size) override;

    std::vector<uia::comm::endpoint> local_endpoints() override;
    inline uint16_t local_port() override {
        return port_;
    }
    inline std::string error_string() override {
        return "";
    }
    using uia::comm::socket::receive;
};

} // simulation namespace
} // ssu namespace
