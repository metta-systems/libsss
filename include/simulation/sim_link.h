//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include "link.h"

namespace ssu {

class host;

namespace simulation {

class sim_host;
class simulator;

class sim_link : public link
{
    std::shared_ptr<simulator> simulator_;
    std::shared_ptr<sim_host> host_;
    uint16_t port_{0};

public:
    sim_link(std::shared_ptr<host> host);
    ~sim_link();

    bool send(const endpoint& ep, const char *data, size_t size) override;

    // void bind() override;
    // void unbind() override;

    std::vector<endpoint> local_endpoints() override;
};

} // simulation namespace
} // ssu namespace
