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

class byte_array;

namespace uia {
namespace comm {

class socket_host_interface;
class socket_endpoint;

/**
 * Abstract base class for packet receivers.
 * Provides support for receiving messages for registered types.
 */
class packet_receiver : public std::enable_shared_from_this<packet_receiver>
{
    socket_host_interface* host_interface_{nullptr};
    std::string magic_;

protected:
    inline packet_receiver(socket_host_interface* hi)
        : host_interface_(hi)
    {
    }

    inline packet_receiver(socket_host_interface* hi, std::string magic)
        : host_interface_(hi)
    {
        bind(magic);
    }

    inline ~packet_receiver() { unbind(); }

    void bind(std::string magic);
    void unbind();

    inline std::string magic() const { return magic_; }

    inline bool is_bound() const { return !magic_.empty(); }

    // @fixme Possibly set_magic() might set a magic on default packet_receiver and bind() it.
    // just use bind(magic);

public:
    /**
     * Socket calls this method to dispatch packets.
     * @param msg Data packet.
     * @param src Origin endpoint.
     */
    virtual void receive(boost::asio::const_buffer msg, uia::comm::socket_endpoint const& src) = 0;
};

} // comm namespace
} // uia namespace
