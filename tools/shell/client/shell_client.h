//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "shell_protocol.h"
#include "shell_stream.h"
#include "async_file.h"
#include "ssu/host.h"

class shell_client : public shell_protocol
{
private:
    std::shared_ptr<ssu::stream> stream_;
    shell_stream shs;
    async_file afin, afout;

public:
    shell_client(std::shared_ptr<ssu::host> host);

    inline void connect_to(const ssu::peer_id& dst_eid, const ssu::endpoint& location_hint)
    {
        assert(!stream_->is_connected());
        stream_->connect_to(dst_eid, service_name, protocol_name, location_hint);
    }

    inline void connect_at(ssu::endpoint const& ep)
    {
        stream_->connect_at(ep);
    }

    void setup_terminal(int fd);
    void run_shell(std::string const& cmd, int infd, int outfd);

private:
    void got_control_packet(byte_array const& msg);

    // Handlers
    void in_ready();
    void out_ready();
};
