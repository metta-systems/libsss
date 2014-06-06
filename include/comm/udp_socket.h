//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <memory>
#include <boost/asio.hpp>
#include "comm/socket.h"

class settings_provider;

namespace ssu {

class host;

/**
 * Class for UDP connection between two endpoints.
 * Multiplexes between channel-setup/key exchange traffic (which goes to ssu::key_responder)
 * and per-channel data traffic (which goes to ssu::channel).
 */
class udp_socket : public uia::comm::socket
{
    /**
     * Underlying socket.
     */
    boost::asio::ip::udp::socket udp_socket_;
    // boost::asio::ip::udp::socket udp6_socket; ///< ipv6 socket - host manages two udp_sockets instead
    boost::asio::streambuf received_buffer_;
    /**
     * Endpoint we've received the packet from.
     */
    uia::comm::socket_endpoint received_from_;
    /**
     * Network activity execution queue.
     */
    boost::asio::strand strand_;
    /**
     * Socket error status.
     */
    std::string error_string_;

public:
    udp_socket(std::shared_ptr<host> host);

    /**
     * Bind this UDP socket to a port and activate it if successful.
     * @param  ep Local endpoint to bind to.
     * @return    true if bind successful and socket has been activated, false otherwise.
     */
    bool bind(uia::comm::endpoint const& ep) override;
    void unbind() override;

    /**
     * Send a packet on this UDP socket.
     * @param  ep   Target endpoint - intended receiver of the packet.
     * @param  data Packet data.
     * @param  size Packet size.
     * @return      If send was successful, i.e. the packet has been sent. It does not say anything
     *              about the reception of the packet on the other side, if it was ever delivered
     *              or accepted.
     */
    bool send(uia::comm::endpoint const& ep, char const* data, size_t size) override;
    using uia::comm::socket::send;

    /**
     * Return a description of any error detected on bind() or send().
     */
    inline std::string error_string() override { return error_string_; }

    /**
     * Return all known local endpoints referring to this socket.
     */
    std::vector<uia::comm::endpoint> local_endpoints() override;

    uint16_t local_port() override;

private:
    void prepare_async_receive();
    void udp_ready_read(const boost::system::error_code& error, std::size_t bytes_transferred);
};

/**
 * Helper function to bind a passed in socket to a given ep and set the error string to
 * occured error if any.
 * @param  sock         UDP socket to open and bind.
 * @param  ep           Endpoint to bind to. Can be ipv4 or ipv6.
 * @param  error_string Output string to set if error occured.
 * @return              true if successful, false if any error occured. Error string is set then.
 */
bool bind_socket(boost::asio::ip::udp::socket& sock,
    uia::comm::endpoint const& ep, std::string& error_string);

} // ssu namespace

