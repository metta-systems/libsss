//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <queue>
#include <boost/signals2/signal.hpp>
#include "arsenal/byte_array.h"
#include "sss/host.h"
#include "sss/channels/peer_identity.h"
#include "sss/stream.h"

namespace sss {

class base_stream;

/**
 * This class represents a server that can accept incoming SSS connections.
 *
 * To use this class, the application creates a server instance, calls listen() to begin
 * listening for connections, and upon arrival of a on_new_connection() signal uses accept()
 * to accept any queued incoming connections.
 */
class server : public stream_protocol
{
    friend class base_stream; /// @fixme for enqueueing streams to accept

    std::shared_ptr<host> host_;
    std::queue<std::shared_ptr<base_stream>> received_connections_; // Received connection stream queue
    std::string service_name_, service_description_;
    std::string protocol_name_, protocol_description_;
    std::string error_string_;
    bool active_{false};

public:
    /**
     * Create a server instance.
     * The application must call listen() before the server will actually
     * accept incoming connections.
     * @param host the host object containing hostwide SSS state.
     */
    server(std::shared_ptr<host> host);

    /**
     * Listen for incoming connections to a particular service using a particular
     * application protocol. This method may only be called once on a server instance.
     * An error occurs if another server object is already listening on the specified
     * service/protocol name pair on this host.
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

    /**
     * Returns true if this server is currently listening.
     */
    inline bool is_listening() const { return active_; }

    /**
     * Accept an incoming connection as a top-level stream.
     * Upon receiving the on_new_connection signal, the application must call accept() in a loop
     * until there are no more incoming connections to accept.
     *
     * @snippet doc/snippets.cpp Accepting a connection
     *
     * @return a new stream representing ths incoming connection,
     *         or nullptr if no connections are currently waiting.
     */
    std::shared_ptr<stream> accept();

    /**
     * Accept an incoming connection, and obtain the EID of the originating host.
     * @overload
     */
    inline std::shared_ptr<stream> accept(uia::peer_identity& from_host)
    {
        auto stream = accept();
        if (stream) {
            from_host = stream->remote_host_id();
        }
        return stream;
    }

    /**
     * Returns the service name previously supplied to listen().
     */
    inline std::string service_name() { return service_name_; }

    /**
     * Returns the service description previously supplied to listen().
     */
    inline std::string service_description() { return service_description_; }

    /**
     * Returns the protocol name previously supplied to listen().
     */
    inline std::string protocol_name() { return protocol_name_; }

    /**
     * Returns the protocol description previously supplied to listen().
     */
    inline std::string protocol_description() { return protocol_description_; }

    /**
     * Returns a string describing the last error that occurred, if any.
     */
    inline std::string error_string() { return error_string_; }

    /** @name Signals. */
    /**@{*/
    using connection_notify_signal = boost::signals2::signal<void(void)>;
    /**
     * Emitted when a new connection arrives. To prevent races accept
     * all incoming streams in the handler function as described in accept() documentation.
     */
    connection_notify_signal on_new_connection;
    /**@}*/

protected:
    /**
     * Set the current error description string.
     * @param err Textual error description.
     */
    inline void set_error_string(std::string const& err) { error_string_ = err; }
};

} // sss namespace
