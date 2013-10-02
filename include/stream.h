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
#include "peer_id.h"
#include "link.h"
#include "negotiation/key_responder.h"

namespace ssu {

class host;
class base_stream;
class stream_peer;
class channel;
class server;

/**
 * User-space interface to the stream.
 *
 * This is the primary high-level class that client applications use to communicate over
 * the network via SSU. The class provides standard stream-oriented read/write functionality
 * and adds SSU-specific methods for controlling SSU streams and substreams.
 *
 * To initiate an outgoing "top-level" SSU stream to a remote host, the client application
 * creates a stream instance and then calls connect_to().
 * 
 * To initiate a sub-stream from an existing stream, the application calls
 * open_substream() on the parent stream.
 *
 * To accept incoming top-level streams from other hosts the application creates
 * a ssu::server instance, and that class creates stream instances for incoming
 * connections.
 * 
 * To accept new incoming substreams on existing streams, the application calls
 * listen() on the parent stream, and upon arrival of a new_substream() signal
 * the application calls accept_substream() to obtain a stream object for the
 * new incoming substream.
 *
 * SSU uses service and protocol names in place of the port numbers used
 * by TCP and UDP to differentiate and select among different application
 * protocols.
 * 
 * A service name represents an abstract service being provided: e.g., "Web",
 * "File", "E-mail", etc. A protocol name represents a concrete application
 * protocol to be used for communication with an abstract service: e.g.,
 * "HTTP 1.0" or "HTTP 1.1" for communication with a "Web" service; "FTP",
 * "NFS v4", or "CIFS" for communication with a "File" service; "SMTP", "POP3",
 * or "IMAP4" for communication with an "E-mail" service.
 * 
 * Service names are intended to be suitable for non-technical users to see, in
 * a service manager or firewall configuration utility for example, while
 * protocol names are primarily intended for application developers.
 * 
 * A server can support multiple distinct protocols on one logical service,
 * for backward compatibility or functional modularity reasons for example,
 * by registering to listen on multiple (service, protocol) name pairs.
 *
 * @see server
 */
class stream : public std::enable_shared_from_this<stream>
{
    friend class abstract_stream; // @todo get rid of this. only used for set_error()
    friend class base_stream; // @todo get rid of this.

    std::shared_ptr<host> host_;
    base_stream* stream_{nullptr};

    stream(base_stream* other_stream); friend class server;

public:
    /**
     * Flag bits used as arguments to the listen() method, indicating when and
     * how to accept incoming substreams.
     */
    enum listen_mode
    {
        // @todo Clean up these values
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

    stream(std::shared_ptr<host> h);
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
    bool connect_to(peer_id const& destination,
        std::string service, std::string protocol,
        endpoint const& destination_endpoint_hint = endpoint());

    /**
     * Disconnect the stream from its current peer.
     * 
     * This method immediately returns the stream to the unconnected state:
     * is_connected() subsequently returns false.
     * 
     * If the stream has not already been shutdown, however, SSU gracefully closes the stream
     * in the background as if with shutdown(close).
     * 
     * @see shutdown()
     */
    void disconnect();

    /**
     * Returns true if the Stream is logically connected
     * and usable for data read/write operations.
     * The return value from this method changes only as a result of
     * the application's calls to connectTo() and disconnect().
     * Logical connectivity does not imply that the network link is live:
     * the underlying link may go up or down repeatedly
     * during the logical lifetime of the stream.
     */
    bool is_connected() const;

    /**
     * Provide a new or additional peer address/location hint.
     * May be called at any time, e.g., if the target host has migrated,
     * to give SST a hint at where it might find the target
     * in order to re-establish connectivity.
     */
    void connect_at(endpoint const& ep);

    //===============================================================
    // Byte-oriented data transfer.
    // Reading data.
    //===============================================================

    /**
     * Determine the number of bytes currently available to be read via readData().
     * Note that calling readData() with a buffer this large may not read all
     * the available data if there are message/record markers present in the read stream.
     * @return Number of bytes available.
     */
    size_t bytes_available() const;

    /// Returns true if at least one byte is available for reading.
    inline bool has_bytes_available() const {
        return bytes_available() > 0;
    }

    /**
     * Returns true if all data has been read from the stream
     * and the remote host has closed its end:
     * no more data will ever be available for reading on this stream.
     */
    bool at_end() const; //XXX QIODevice relic

    /**
     * Read up to maxSize bytes of data from the stream.
     * This method only returns data that has already been received
     * and is waiting to be read:
     * it never blocks waiting for new data to arrive on the network.
     * A single readData() call never crosses a message/record boundary:
     * if it encounters a record marker in the incoming byte stream,
     * it returns only the bytes up to that record marker
     * and leaves any further data
     * for subsequent readData() calls.
     *
     * @param data the buffer into which to read.
     *      This parameter may be NULL,
     *      in which case the data read is simply discarded.
     * @param maxSize the maximum number of bytes to read.
     * @return the number of bytes read, or -1 if an error occurred.
     *      Returns zero if there is no error condition
     *      but no bytes are immediately available for reading.
     */
    ssize_t read_data(char* data, size_t max_size);

    /**
     * Read up to maxSize bytes of data into a QByteArray.
     * @overload
     */
    byte_array read_data(size_t max_size = 1 << 30);

    /**
     * Return number of complete records currently available for reading.
     */
    int pending_records() const;

    /**
     * Return true if at least one complete record is currently available for reading.
     */
    inline bool has_pending_records() const {
        return pending_records() > 0;
    }

    /**
     * Read a complete message all at once.
     * Reads up to the next message/record marker (or end of stream).
     * If no message/record marker has arrived yet,
     * just returns without reading anything.
     * If the next message to be read is larger than maxSize,
     * this method simply discards the message data beyond maxSize.
     * @param data the buffer into which to read the message.
     * @param maxSize the maximum size of the message to read.
     * @return the size of the message read, or -1 if an error occurred.
     *      Returns zero if there is no error condition
     *      but no complete message is available for reading.
     */
    ssize_t read_record(char* data, size_t max_size);

    /**
     * Read a complete message into a new QByteArray.
     * @param maxSize the maximum size of the message to read;
     *      any bytes in the message beyond this are discarded.
     * @return the message received,
     *      or an empty QByteArray if an error occurred
     *      or there are no messages to receive.
     * @overload
     */
    byte_array read_record(size_t max_size = 1 << 30);

    //===============================================================
    // Byte-oriented data transfer.
    // Writing data.
    //===============================================================

    /** Write data bytes to a stream.
     * If not all the supplied data can be transmitted immediately,
     * it is queued locally until ready to transmit.
     * @param data the buffer containing the bytes to write.
     * @param size the number of bytes to write.
     * @return the number of bytes written (same as the size parameter),
     *      or -1 if an error occurred.
     */
    ssize_t write_data(const char* data, size_t size);

    /** Write a message to a stream.
     * Writes the data in the supplied buffer
     * followed by a message/record marker.
     * If some data has already been written via writeData(),
     * then that data logically forms the "head" of the message
     * and the data presented to writeMessage() forms the "tail".
     * Thus, a large message can be written incrementally
     * by calling writeData() any number of times
     * followed by a call to writeMessage() to finish the message.
     * A message/record marker is written at the current position
     * even if this method is called with no data (size = 0).
     * @param data the buffer containing the message to write.
     * @param size the number of bytes of data to write.
     * @return the number of bytes written (same as the size parameter),
     *      or -1 if an error occurred.
     */
    ssize_t write_record(const char* data, size_t size);

    /** Write a message to a stream.
     * @param msg a QByteArray containing the message to write.
     * @overload
     */
    inline ssize_t write_record(const byte_array& rec) {
        return write_record(rec.data(), rec.size());
    }

    //===============================================================
    // Datagram protocol.
    // Send and receive unordered, unreliable datagrams on this stream.
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

    /// Check for pending datagrams
    bool has_pending_datagrams() const;
    ssize_t pending_datagram_size() const;

    //===============================================================
    // Substreams management.
    //===============================================================

    /**
     * Initiate a new substream as a child of this stream.
     * This method completes without synchronizing with the remote host,
     * and the client application can use the new substream immediately
     * to send data to the remote host via the new substream.
     * If the remote host is not yet ready to accept the new substream,
     * SSU queues the new substream and any data written to it locally
     * until the remote host is ready to accept the new substream.
     *
     * @return A stream object representing the new substream.
     */
    stream* open_substream();

    /**
     * Listen for incoming substreams on this stream.
     */
    void listen(listen_mode mode);

    /**
     * Returns true if this stream is set to accept incoming substreams.
     */
    bool is_listening() const;

    /**
     * Accept a waiting incoming substream.
     *
     * @return NULL if no incoming substreams are waiting.
     */
    stream* accept_substream();

    //===============================================================
    // Stream control.
    //===============================================================

    /**
     * Returns the endpoint identifier of the local host
     * as used in connecting the current stream.
     * Only valid if the stream is connected.
     */
    peer_id local_host_id() const;

    /**
     * Returns the endpoint identifier of the remote host
     * to which this stream is connected.
     */
    peer_id remote_host_id() const;

    /**
     * Returns true if the underlying link is currently connected
     * and usable for data transfer.
     */
    bool is_link_up() const;

    /** 
     * Set the stream's transmit priority level.
     * When the application has multiple streams
     * with data ready to transmit to the same remote host,
     * SST uses the respective streams' priority levels
     * to determine which data to transmit first.
     * SST gives strict preference to streams with higher priority
     * over streams with lower priority,
     * but it divides available transmit bandwidth evenly
     * among streams with the same priority level.
     * All streams have a default priority of zero on creation.
     * @param priority the new priority level for the stream.
     */
    void set_priority(int priority);

    /**
     * Returns the stream's current priority level.
     */
    int current_priority() const;

    /**
     * Begin graceful or forceful shutdown of the stream.
     * If this internal stream control object is lingering - i.e., if its 'owner_'
     * back-pointer is NULL - then it should self-destruct once the shutdown is complete.
     *
     * To close the stream gracefully in either or both directions, specify read, write,
     * or read|write for the @a mode argument.
     * Closing the stream in the write direction writes the end-of-stream marker to our
     * end of the stream, indicating to our peer that no more data will arrive from us.
     * Closing the stream in the read direction discards any data waiting to be read or
     * subsequently received from the peer.
     * Specify a mode of @a reset to shutdown the stream immediately;
     * written data that is still queued or in transit may be lost.
     *
     * @param mode which part of the stream to close:
     *      either read, write, close (read|write), or reset.
     */
    void shutdown(shutdown_mode mode);

    /// Gracefully close the stream for both reading and writing.
    /// Still-buffered written data continues to be sent,
    /// but any further data received from the other side is dropped.
    inline void close() { shutdown(shutdown_mode::close); }

    /// Control the receive buffer size for this stream.
    void set_receive_buffer_size(size_t size);
    /// Control the initial receive buffer size for new child streams.
    void set_child_receive_buffer_size(size_t size);

    /**
     * Give the stream layer a location hint for a specific EID,
     * which may or may not be the EID of the host
     * to which this stream is currently connected (if any).
     * The stream layer will use this hint in any current or subsequent
     * attempts to connect to the specified EID.
     */
    bool add_location_hint(peer_id const& eid, endpoint const& hint);

    /// Dump the state of this stream, for debugging purposes.
    void dump();

    //===============================================================
    // Signals.
    //===============================================================

    /**
     * Emitted when some locally buffered data gets flushed
     * after being delivered to the receiver and acknowledged.
     */
    typedef boost::signals2::signal<void (ssize_t)> bytes_written_signal;
    bytes_written_signal on_bytes_written;

    typedef boost::signals2::signal<void (void)> ready_signal;
    ready_signal on_ready_read; // QIODevice
    ready_signal on_ready_read_record;
    ready_signal on_ready_read_datagram;
    ready_signal on_ready_write;
    ready_signal on_receive_blocked;

    typedef boost::signals2::signal<void (void)> link_status_change_signal;
    link_status_change_signal on_link_up;
    link_status_change_signal on_link_stalled;
    link_status_change_signal on_link_down;

    typedef boost::signals2::signal<void (void)> substream_notify_signal;
    substream_notify_signal on_new_substream;

    typedef boost::signals2::signal<void (const std::string&)> error_signal;
    error_signal on_error_notify;
    error_signal on_reset_notify;

protected:
    // Set an error condition on this stream and emit the error_notify signal.
    void set_error(const std::string& error);
};

/**
 * Private helper class, to register with link layer to receive key exchange packets.
 * Only one instance ever created per host.
 */
class stream_responder : public negotiation::key_responder, public stream_protocol
{
    friend class stream_host_state;

    stream_responder(std::shared_ptr<host> host);

    channel* create_channel(link_endpoint const& initiator_ep,
            byte_array const& initiator_eid,
            byte_array const& user_data_in, byte_array& user_data_out) override;
};

} // ssu namespace
