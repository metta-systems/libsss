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

class abstract_stream;

/**
 * User-space interface to the stream.
 *
 * This is the primary high-level class that client applications use to
 * communicate over the network via SSU. The class provides standard
 * stream-oriented read/write functionality via the methods in its
 * QIODevice base class, and adds SSU-specific methods for controlling SSU
 * streams and substreams.
 *
 * To initiate an outgoing "top-level" SSU stream to a remote host, the client
 * application creates a stream instance and then calls connect_to().
 * To initiate a sub-stream from an existing stream, the application calls
 * open_substream() on the parent stream.
 *
 * To accept incoming top-level streams from other hosts the application creates
 * a ssu::server instance, and that class creates stream instances for incoming
 * connections.
 * To accept new incoming substreams on existing streams, the application calls
 * listen() on the parent stream, and upon arrival of a new_substream() signal
 * the application calls accept_substream() to obtain a stream object for the
 * new incoming substream.
 *
 * SSU uses service and protocol names in place of the port numbers used
 * by TCP and UDP to differentiate and select among different application
 * protocols.
 * A service name represents an abstract service being provided: e.g., "Web",
 * "File", "E-mail", etc. A protocol name represents a concrete application
 * protocol to be used for communication with an abstract service: e.g.,
 * "HTTP 1.0" or "HTTP 1.1" for communication with a "Web" service; "FTP",
 * "NFS v4", or "CIFS" for communication with a "File" service; "SMTP", "POP3",
 * or "IMAP4" for communication with an "E-mail" service.
 * Service names are intended to be suitable for non-technical users to see, in
 * a service manager or firewall configuration utility for example, while
 * protocol names are primarily intended for application developers.
 * A server can support multiple distinct protocols on one logical service,
 * for backward compatibility or functional modularity reasons for example,
 * by registering to listen on multiple (service, protocol) name pairs.
 *
 * @see server
 */
class stream : public std::enable_shared_from_this<stream>
{
    abstract_stream* stream_{nullptr};
    std::weak_ptr<host> host_;

    stream(abstract_stream* other_stream);

public:
    typedef uint16_t id_t;      ///< Stream ID within channel.
    typedef uint32_t byteseq_t; ///< Stream byte sequence number.
    typedef uint64_t counter_t; ///< Counter for SID assignment.

    /**
     * Flag bits used as arguments to the listen() method, indicating when and
     * how to accept incoming substreams.
     */
    enum class listen_mode
    {
        reject         = 0,    ///< Reject incoming substreams.
        buffer_limit   = 2,    ///< Accept subs up to receive buffer size.
        unlimited      = 3,    ///< Accept substreams of any size.
        inherit        = 4,    ///< Flag: Substreams inherit this listen mode.
    };

    /**
     * Flag bits used as operands to the shutdown() method, indicating how and
     * in which direction(s) to shutdown a stream.
     */
    enum class shutdown_mode
    {
        read    = 1,    ///< Read (incoming data) direction.
        write   = 2,    ///< Write (outgoing data) direction.
        close   = 3,    ///< Both directions (Read|Write).
        reset   = 4,    ///< Forceful reset.
    };

    /**
     * Type for identifying streams uniquely across channels.
     *
     * XXX should contain a "keying method identifier" of some kind?
     */
    struct unique_stream_id_t
    {
        counter_t counter; ///< Stream counter in channel
        byte_array half_channel_id; ///< Unique channel+direction ID 
                                    ///< ("half-channel id")
    };

    stream(std::shared_ptr<host>& h);
    virtual ~stream();

    //===============================================================
    // Connection-related services.
    //===============================================================
    
    /**
     * Connect to a given service and protocol on a remote host.
     * The stream logically goes into the "connected" state immediately
     * (i.e., isConnected() returns true),
     * and the application may start writing to the stream immediately,
     * but actual network connectivity may not be established
     * until later or not at all.
     * Either during or some time after the connectTo() call,
     * SST emits the linkUp() signal to indicate active connectivity,
     * or linkDown() to indicate connectivity could not be established.
     * A linkDown() signal is not necessarily fatal, however:
     * unless the application disconnects or deletes the Stream object,
     * SST will continue attempting to establish connectivity
     * and emit linkUp() if and when it eventually succeeds.
     *
     * If the stream is already connected when connectTo() is called,
     * SST immediately re-binds the Stream object to the new target
     * but closes the old stream gracefully in the background.
     * Similarly, SST closes the stream gracefully in the background
     * if the application just deletes a connected Stream object.
     * To close a stream forcefully without retaining internal state,
     * the application may explicitly call shutdown(Reset)
     * before re-connecting or deleting the Stream object.
     * 
     * @param  destination The endpoint identifier (EID)
     *      of the desired remote host to connect to.
     *      The destination may be either a cryptographic EID
     *      or a non-cryptographic legacy address
     *      as defined by the Ident class.
     *      The destination may also be empty,
     *      indicating that the destination's identity is unknown;
     *      in this case the caller must provide a location hint
     *      via the destination_endpoint_hint argument.
     * @param  service The service name to connect to on the remote host.
     * @param  protocol The application protocol name to connect to.
     * @param  destination_endpoint_hint An optional location hint
     *      for SST to use in attempting to contact the host.
     *      If the dstid parameter is a cryptographic EID,
     *      which is inherently location-independent,
     *      SST may need a location hint to find the remote host
     *      if this host and the remote host are not currently
     *      registered at a common registration server,
     *      for example.
     *      This parameter is not needed
     *      if the dstid is a non-cryptographic legacy address.
     * @return true if successful, false if an error occurred.
     */
    bool connect_to(peer_id& destination, 
        std::string service, std::string protocol,
        const endpoint& destination_endpoint_hint = endpoint());

    /** Disconnect the stream from its current peer.
     * This method immediately returns the stream to the unconnected state:
     * isConnected() subsequently returns false.
     * If the stream has not already been shutdown, however,
     * SST gracefully closes the stream in the background
     * as if with shutdown(Close).
     * @see shutdown()
     */
    void disconnect();

    bool is_connected() const;

    //===============================================================
    // Reading data.
    //===============================================================

    size_t bytes_available() const;
    bool has_bytes_available() const {
        return bytes_available() > 0;
    }

    virtual ssize_t read_data(char* data, size_t max_size);
    byte_array read_data(size_t max_size = 1 << 30);

    int pending_records() const;
    inline bool has_pending_records() const {
        return pending_records() > 0;
    }

    ssize_t read_record(char* data, size_t max_size);
    byte_array read_record(size_t max_size = 1 << 30);

    bool at_end() const; //XXX QIODevice relic

    //===============================================================
    // Writing data.
    //===============================================================

    ssize_t write_data(const char* data, size_t size);
    ssize_t write_record(const char* data, size_t size);
    inline ssize_t write_record(const byte_array& msg) {
        return write_record(msg.data(), msg.size());
    }

    //===============================================================
    // Datagram protocol.
    //===============================================================

    enum class datagram_type
    {
        non_reliable = 0,
        reliable = 1
    };

    ssize_t read_datagram(char* data, size_t max_size);
    byte_array read_datagram(size_t max_size = 1 << 30);
    ssize_t write_datagram(const char* data, size_t size,
        datagram_type is_reliable);
    inline ssize_t write_datagram(const byte_array& dgm,
        datagram_type is_reliable)
    {
        return write_datagram(dgm.data(), dgm.size(), is_reliable);
    }

    bool has_pending_datagrams() const;
    ssize_t pending_datagram_size() const;

    //===============================================================
    // Substreams management.
    //===============================================================

    stream* open_substream();

    void listen();
    bool is_listening() const;

    stream* accept_substream();

    //===============================================================
    // Stream control.
    //===============================================================

    peer_id local_host_id() const;
    peer_id remote_host_id() const;
    bool is_link_up() const;

    void set_priority(int prio);
    int priority() const;

    void set_receive_buffer_size(size_t size);
    void set_child_receive_buffer_size(size_t size);

    void shutdown(shutdown_mode mode);
    inline void close() { shutdown(shutdown_mode::close); }

    bool add_location_hint(const peer_id& eid, const endpoint& hint);

    //===============================================================
    // Signals.
    //===============================================================

    typedef boost::signals2::signal<void(void)> ready_signal;
    ready_signal ready_read_message;
    ready_signal ready_read_datagram;
    ready_signal ready_write;
    ready_signal receive_blocked;

    typedef boost::signals2::signal<void(void)> link_status_change_signal;
    link_status_change_signal link_up;
    link_status_change_signal link_stalled;
    link_status_change_signal link_down;

    typedef boost::signals2::signal<void(void)> substream_notify_signal;
    substream_notify_signal new_substream;

    typedef boost::signals2::signal<void(const std::string&)> error_signal;
    error_signal error_notify;
    error_signal reset_notify;
};

} // namespace ssu
