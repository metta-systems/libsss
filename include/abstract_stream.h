//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include "byte_array.h"
#include "protocol.h"
#include "peer_id.h"
#include "stream.h"
#include "underlying.h"

namespace ssu {

class host;
class stream;
class stream_peer;

/** 
 * @internal
 * Abstract base class for internal stream control objects.
 * 
 * The separation between the internal stream control object and the
 * application-visible stream object is primarily needed so that ssu can
 * hold onto a stream's state and gracefully shut it down after the
 * application deletes its stream object representing it.
 * This separation also keeps the internal stream control variables out of the
 * public C++ API header files and thus able to change without breaking binary
 * compatibility, and makes it easy to implement service/protocol negotiation
 * for top-level application streams by extending this class.
 *
 * @see base_stream, connect_stream
 */
class abstract_stream : public stream_protocol
{
    friend class stream;

protected:
    std::shared_ptr<host> host_;    ///< Per-host state.
    std::weak_ptr<stream> owner_;   ///< Back-pointer to stream object, 
                                    ///< or nullptr if stream has been deleted.
    peer_id peerid_;                ///< EID of peer we're connected to.

private:
    int                 priority_;    ///< Current priority level
    stream::listen_mode listen_mode_; ///< Listen for substreams.

public:
    /**
     * Create a new abstract_stream.
     */
    abstract_stream(std::shared_ptr<host> h);

    //===============================================================
    // Byte-oriented data transfer.
    // Reading data.
    //===============================================================

    virtual ssize_t bytes_available() const = 0;
    inline bool has_bytes_available() const {
        return bytes_available() > 0;
    }

    virtual bool at_end() const = 0; //XXX QIODevice relic

    virtual ssize_t read_data(char* data, ssize_t max_size) = 0;

    /**
     * Return number of complete records currently available for reading.
     */
    virtual int pending_records() const = 0;

    /**
     * Return true if at least one complete record is currently available for reading.
     */
    inline bool has_pending_records() const {
        return pending_records() > 0;
    }

    virtual ssize_t pending_record_size() const = 0;

    virtual ssize_t read_record(char* data, ssize_t max_size) = 0;
    virtual byte_array read_record(ssize_t max_size) = 0;

    //===============================================================
    // Byte-oriented data transfer.
    // Writing data.
    //===============================================================

    virtual ssize_t write_data(const char* data, ssize_t size, uint8_t endflags) = 0;

    virtual ssize_t write_record(const char* data, ssize_t size) {
        return write_data(data, size, flags::data_message);
    }

    virtual ssize_t write_record(const byte_array& rec) {
        return write_record(rec.data(), rec.size());
    }

    //===============================================================
    // Datagram protocol.
    // Send and receive unordered, unreliable datagrams on this stream.
    //===============================================================
    virtual ssize_t read_datagram(char* data, ssize_t max_size) = 0;
    virtual ssize_t write_datagram(const char* data, ssize_t size, stream::datagram_type is_reliable) = 0;
    virtual byte_array read_datagram(ssize_t max_size) = 0;

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
    virtual std::shared_ptr<abstract_stream> open_substream() = 0;

    /**
     * Listen for incoming substreams on this stream.
     */
    inline void listen(stream::listen_mode mode) {
        listen_mode_ = mode;
    }

    inline stream::listen_mode listen_mode() const {
        return listen_mode_;
    }

    /**
     * Returns true if this stream is set to accept incoming substreams.
     */
    inline bool is_listening() const {
        return listen_mode_ != stream::listen_mode::reject;
    }

    /**
     * Accept a waiting incoming substream.
     *
     * @return NULL if no incoming substreams are waiting.
     */
    virtual std::shared_ptr<abstract_stream> accept_substream() = 0;

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
    virtual bool is_link_up() const = 0;

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
     * @param priority the new priority level for the stream,
     *  higher values of priority mean higher transmit preference.
     *  e.g. stream with priority 1 has higher preference than stream
     *  with default priority 0.
     */
    virtual void set_priority(int priority);

    /**
     * Returns the stream's current priority level.
     */
    inline int current_priority() const { return priority_; }

    /**
     * Begin graceful or forceful shutdown of the stream.
     * If this internal stream control object is lingering -
     * i.e., if its 'owner' back-pointer is NULL -
     * then it should self-destruct once the shutdown is complete.
     *
     * @param mode which part of the stream to close:
     *      either Read, Write, Close (Read|Write), or Reset.
     */
    virtual void shutdown(stream::shutdown_mode mode) = 0;

    virtual void set_receive_buffer_size(size_t size) = 0;
    virtual void set_child_receive_buffer_size(size_t size) = 0;

    /**
     * Dump the state of this stream, for debugging purposes.
     */
    virtual void dump() = 0;

    //-------------------------------------------
    // Signals
    //-------------------------------------------

    /**
     * A complete record has been received.
     */
    boost::signals2::signal<void()> on_ready_read_record;

protected:
    void set_error(const std::string& error) {
        if (auto strm = owner_.lock()) {
            strm->set_error(error);
        }
    }
};

} // namespace ssu
