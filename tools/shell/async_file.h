//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <deque>
#include <boost/asio.hpp> // @todo More precise include, please
#include <boost/signals2/signal.hpp>
#include "byte_array.h"

/**
 * Generic asynchronous I/O wrapper for using native file descriptors
 * in nonblocking mode on both reads and writes.
 * Provides internal write buffering so that writes always succeed
 * even if the underlying file descriptor isn't ready to accept data.
 * (The caller can use bytes_to_write() to see if output is blocked.)
 */
class async_file
{
public:
    enum Status { Ok, Error };
    enum OpenMode {Read=1, Write=2, ReadWrite=3};

private:
    /**
     * See http://boost.cowic.de/rc/pdf/asio_doc.pdf
     */
    boost::asio::posix::stream_descriptor sd;

    std::deque<byte_array> inq;
    std::deque<byte_array> outq;
    ssize_t outqd;
    int mode_;
    Status st;
    bool endread;
    std::string error_string_;

public:
    async_file(boost::asio::io_service& service);
    ~async_file();

    bool open(int fd, OpenMode mode);
    void close();
    void close_read();
    void close_write();

    /// Returns true if we have reached end-of-file in the input direction.
    bool at_end() const;

    // inline Status status() { return st; }

    //========================================

    inline bool is_open() const { return sd.is_open(); }

    /// Returns the number of bytes currently queued to write
    /// but not yet written to the underlying file descriptor.
    inline ssize_t bytes_to_write() const { return outqd; }

    boost::signals2::signal<void (void)> on_ready_read;
    boost::signals2::signal<void (ssize_t)> on_bytes_written;

    // Basic stream io interface.
    ssize_t read(char* buf, ssize_t max_size);
    byte_array read(ssize_t max_size);

    ssize_t write(char const* buf, ssize_t size);
    ssize_t write(byte_array const& buf);

    //========================================

    void set_error(std::string const& msg);
    std::string error_string() const { return error_string_; }

protected:
    bool open(OpenMode mode);
    void read_some(boost::system::error_code const& error, std::size_t bytes_transferred);
    void write_some(boost::system::error_code const& error, std::size_t bytes_transferred);

private: //slots
    // void ready_write();
};
