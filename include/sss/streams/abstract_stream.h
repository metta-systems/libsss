//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include "arsenal/byte_array.h"
#include "arsenal/underlying.h"
#include "sss/framing/stream_protocol.h"
#include "sss/channels/peer_identity.h"
#include "sss/stream.h"

namespace sss {

class host;
class stream;
namespace internal {
class stream_peer;
}

/**
 * @internal
 * Abstract base class for internal stream control objects.
 *
 * The separation between the internal stream control object and the
 * application-visible stream object is primarily needed so that sss can
 * hold onto a stream's state and gracefully shut it down after the
 * application deletes its stream object representing it.
 * This separation also keeps the internal stream control variables out of the
 * public C++ API header files and thus able to change without breaking binary
 * compatibility, and makes it easy to implement service/protocol negotiation
 * for top-level application streams by extending this class.
 *
 * @see base_stream
 */
class abstract_stream : public stream_protocol
{
    friend class stream;

protected:
    std::shared_ptr<host> host_;  ///< Per-host state.
    std::weak_ptr<stream> owner_; ///< Back-pointer to stream object,
                                  ///< or nullptr if stream has been deleted.
    uia::peer_identity peer_id_;  ///< EID of peer we're connected to.

public:
    /**
     * When the application has multiple streams with data ready to transmit to the same
     * remote host, SSS uses the respective streams' priority levels to determine which data
     * to transmit first.
     * SSS gives strict preference to streams with higher priority over streams with lower priority,
     * but it divides available transmit bandwidth evenly among streams with the same priority
     * level. All streams have a default priority of one on creation.
     * Substreams priority is relative to parent stream, so that parent stream always sends before
     * child streams. This allows parent stream to guarantee control over child streams.
     */
    using priority_t = uint32_t;

    using ptr      = std::shared_ptr<abstract_stream>;
    using weak_ptr = std::weak_ptr<abstract_stream>;

private:
    priority_t priority_{0};                                       ///< Current priority level
    stream::listen_mode listen_mode_{stream::listen_mode::reject}; ///< Listen for substreams.

public:
    /**
     * Create a new abstract_stream.
     */
    abstract_stream(std::shared_ptr<host> h);

    /**
     * Returns the endpoint identifier of the local host as used in connecting the current stream.
     * Only valid if the stream is connected.
     */
    uia::peer_identity local_host_id() const;

    /**
     * Returns the endpoint identifier of the remote host to which this stream is connected.
     */
    uia::peer_identity remote_host_id() const;

    /**
     * Returns true if the underlying link is currently connected and usable for data transfer.
     */
    virtual bool is_link_up() const = 0;

    /**
     * Set the stream's transmit priority level.
     * @param priority the new priority level for the stream, higher values of priority mean
     *                 higher transmit preference. e.g. stream with priority 2 has higher
     *                 preference than stream with default priority 1.
     * @fixme the priorities are inversed
     */
    virtual void set_priority(priority_t priority);

    /**
     * Returns the stream's current priority level.
     * @fixme if relative to parents, should be calculated hierarchically
     */
    inline priority_t current_priority() const { return priority_; }

    //=============================================================================================
    // Byte-oriented data transfer.
    // Reading data.
    //=============================================================================================

    /**
     * Read up to max_size bytes of data from the stream.
     * This method only returns data that has already been received and is waiting to be read:
     * it never blocks waiting for new data to arrive on the network.
     * A single read_data() call never crosses a record boundary: if it encounters a record
     * marker in the incoming byte stream, it returns only the bytes up to that record marker
     * and leaves any further data for subsequent read_data() calls.
     *
     * @param data the buffer into which to read.
     *      This parameter may be nullptr, in which case the data read is simply discarded.
     * @param max_size the maximum number of bytes to read.
     * @return the number of bytes read, or -1 if an error occurred.
     *      Returns zero if there is no error condition
     *      but no bytes are immediately available for reading.
     */
    virtual ssize_t read_data(char* data, ssize_t max_size) = 0;

    /**
     * Determine the number of bytes currently available to be read via read_data().
     * Note that calling read_data() with a buffer this large may not read all the available data
     * if there are record markers present in the read stream.
     */
    virtual ssize_t bytes_available() const = 0;

    /**
     * Returns true if at least one byte is available for reading.
     */
    inline bool has_bytes_available() const { return bytes_available() > 0; }

    /**
     * Returns true if all data has been read from the stream
     * and the remote host has closed its end:
     * no more data will ever be available for reading on this stream.
     */
    virtual bool at_end() const = 0; // @todo If we wrap it in iostream, this would be eofbit

    //=============================================================================================
    // Byte-oriented data transfer.
    // Writing data.
    //=============================================================================================

    /**
     * Write data bytes to a stream.
     * If not all the supplied data can be transmitted immediately, it is queued locally
     * until ready to transmit.
     * This method is asynchronous w.r.t. the actual packet transmission and returns immediately.
     * @param data the buffer containing the bytes to write.
     * @param size the number of bytes to write.
     * @param endflags flags to finish transmission.
     * @return the number of bytes written (should be the same as the size parameter),
     *      or -1 if an error occurred.
     */
    virtual ssize_t write_data(char const* data, ssize_t size, uint8_t endflags) = 0;

    //=============================================================================================
    // Record-oriented data transfer.
    // Reading data.
    //=============================================================================================

    /**
     * Return number of complete records currently available for reading.
     */
    virtual size_t pending_records() const = 0;

    /**
     * Return true if at least one complete record is currently available for reading.
     */
    inline bool has_pending_records() const { return pending_records() > 0; }

    /**
     * Determine the number of bytes available in a frontmost received record.
     * @return the size of the record to be read next, or -1 if no complete records have been read.
     */
    virtual ssize_t pending_record_size() const = 0;

    /**
     * Read a complete record all at once.
     * Reads up to the next record marker (or end of stream).
     * If no record marker has arrived yet, just returns without reading anything.
     * If the next record to be read is larger than max_size,
     * this method simply discards the record data beyond max_size.
     * @param data the buffer into which to read the record.
     * @param max_size the maximum size of the record to read.
     * @return the size of the record read, or -1 if an error occurred.
     *      Returns zero if there is no error condition
     *      but no complete record is available for reading.
     */
    virtual ssize_t read_record(char* data, ssize_t max_size) = 0;
    /**
     * @overload
     * @return the complete record up to max_size bytes in size,
     *         or empty buffer if error occured or no record available.
     */
    virtual byte_array read_record(ssize_t max_size) = 0;

    //=============================================================================================
    // Record-oriented data transfer.
    // Writing data.
    //=============================================================================================

    virtual ssize_t write_record(char const* data, ssize_t size)
    {
        return 0; // write_data(data, size, flags::data_record);
    }

    virtual ssize_t write_record(byte_array const& rec)
    {
        return write_record(rec.data(), rec.size());
    }

    //=============================================================================================
    // Datagram protocol.
    // Send and receive unordered, unreliable datagrams on this stream.
    // @todo Is it worth extracting into separate datagram_stream completely?
    //=============================================================================================

    virtual ssize_t read_datagram(char* data, ssize_t max_size) = 0;
    virtual byte_array read_datagram(ssize_t max_size) = 0;

    virtual ssize_t
    write_datagram(char const* data, ssize_t size, stream::datagram_type is_reliable) = 0;

    //=============================================================================================
    // Substreams management.
    //=============================================================================================

    /**
     * Initiate a new substream as a child of this stream.
     * This method completes without synchronizing with the remote host,
     * and the client application can use the new substream immediately
     * to send data to the remote host via the new substream.
     * If the remote host is not yet ready to accept the new substream,
     * SSS queues the new substream and any data written to it locally
     * until the remote host is ready to accept the new substream.
     *
     * @return A stream object representing the new substream.
     */
    virtual ptr open_substream(/*priority_t prio = 1*/) = 0;

    /**
     * Listen for incoming substreams on this stream.
     */
    inline void listen(stream::listen_mode mode) { listen_mode_ = mode; }

    inline stream::listen_mode listen_mode() const { return listen_mode_; }

    /**
     * Returns true if this stream is set to accept incoming substreams.
     */
    inline bool is_listening() const { return listen_mode_ != stream::listen_mode::reject; }

    /**
     * Accept a waiting incoming substream.
     *
     * @return nullptr if no incoming substreams are waiting.
     */
    virtual ptr accept_substream() = 0;

    //=============================================================================================
    // Stream control.
    //=============================================================================================

    /**
     * Begin graceful or forceful shutdown of the stream.
     * If this internal stream control object is lingering -
     * i.e., if its 'owner' back-pointer is null -
     * then it should self-destruct once the shutdown is complete.
     *
     * @param mode which part of the stream to close:
     *      either Read, Write, Close (both Read and Write), or Reset.
     */
    virtual void shutdown(stream::shutdown_mode mode) = 0;

    virtual void set_receive_buffer_size(size_t size) = 0;
    virtual void set_child_receive_buffer_size(size_t size) = 0;

    /**
     * Dump the state of this stream, for debugging purposes.
     */
    virtual void dump() = 0;

    //=============================================================================================
    // Signals
    //=============================================================================================

    /**
     * A complete record has been received.
     */
    boost::signals2::signal<void()> on_ready_read_record;

protected:
    /**
     * Set an error condition including an error description string
     * on the user-facing stream object.
     */
    inline void set_error(std::string const& error)
    {
        if (auto stream = owner_.lock()) {
            stream->set_error(error);
        }
    }
};

} // namespace sss
