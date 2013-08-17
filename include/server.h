//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <boost/signals2/signal.hpp>
#include "byte_array.h"
#include "host.h"
#include "peer_id.h"

namespace ssu {

class stream;

/**
 * This class represents a server that can accept incoming SSU connections.
 * 
 * To use this class, the application creates a server instance,
 * calls listen() to begin listening for connections,
 * and upon arrival of a new_connection() signal
 * uses accept() to accept any queued incoming connections.
 */
class server : public stream_protocol
{
    std::weak_ptr<host> host_;
    std::string service_name, service_description;
    std::string protocol_name, protocol_description;
    std::string error_string;

public:
    /**
     * Create a server instance.
     * The application must call listen()
     * before the server will actually accept incoming connections.
     */
    server(std::shared_ptr<host>& host);

    /** 
     * Listen for incoming connections to a particular service
     * using a particular application protocol.
     * This method may only be called once on a server instance.
     * An error occurs if another server object is already listening
     * on the specified service/protocol name pair on this host.
     * @param service_name the service name on which to listen.
     *      Clients must specify the same service name via connect_to() to connect to this server.
     * @param service_desc a short human-readable service description
     *      for use for example by utilities that browse or control the set of services running
     *      on a particular host.
     * @param protocol_name the protocol name on which to listen.
     *      Clients must specify the same protocol name via connect_to() to connect to this server.
     * @param protocol_desc a short human-readable description
     *      of the protocol, useful for browsing the set of protocols a particular service supports.
     * @return true if successful, false if an error occurred.
     */
    bool listen(std::string const& service_name, std::string const& service_desc,
                std::string const& protocol_name, std::string const& protocol_desc);

    bool is_listening() const;

    /**
     * Accept an incoming connection as a top-level stream.
     * Upon receiving the on_new_connection signal, the application must call accept() in a loop
     * until there are no more incoming connections to accept.
     *
     * @todo
     * Stream objects returned from this method
     * initially have the StreamServer as their Qt parent,
     * so they are automatically deleted if the StreamServer is deleted.
     * The application may re-parent these Stream objects if desired,
     * in which case the Stream may outlive the StreamServer object.
     *
     * @return a new stream representing ths incoming connection,
     *         or NULL if no connections are currently waiting.
     */
    stream* accept();

    typedef boost::signals2::signal<void(void)> connection_notify_signal;
    connection_notify_signal on_new_connection;
};

} // namespace ssu
