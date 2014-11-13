//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <boost/signals2/signal.hpp>
#include "sss/abstract_stream.h"

namespace sss {

/**
 * Internal pseudo-stream object representing a received datagram.
 * This class makes ephemeral substreams received via SSS's optimized datagram-oriented
 * delivery mechanism appear to work like a normal stream that was written and closed immediately.
 */
class datagram_stream : public abstract_stream
{
    using super = abstract_stream;

    byte_array const payload_; //!< Application payload of the datagram
    ssize_t pos_;              //!< Current read position in the datagram

    inline ssize_t size() const { return payload_.size(); }
    inline ssize_t remain() const { return size() - pos_; }

public:
    datagram_stream(std::shared_ptr<host> h, byte_array const& data, ssize_t pos)
        : abstract_stream(h)
        , payload_(data)
        , pos_(pos)
    {}
    virtual ~datagram_stream() {}

    bool is_link_up() const override {
        return false; // Already closed.
    }
    void shutdown(stream::shutdown_mode mode) override;

    ssize_t bytes_available() const override {
        return remain();
    }
    bool at_end() const override {
        return pos_ >= size();
    }

    int pending_records() const override {
        return (size() > pos_) ? 1 : 0;
    }
    ssize_t pending_record_size() const override {
        return remain();
    }

    ssize_t read_record(char* data, ssize_t max_size) override;
    byte_array read_record(ssize_t max_size) override;

    ssize_t read_data(char* data, ssize_t max_size) override;
    ssize_t write_data(const char* data, ssize_t size, uint8_t endflags) override;

    std::shared_ptr<abstract_stream> open_substream() override;
    std::shared_ptr<abstract_stream> accept_substream() override;

    ssize_t read_datagram(char* data, ssize_t max_size) override;
    byte_array read_datagram(ssize_t max_size) override;
    ssize_t write_datagram(const char* data, ssize_t size, stream::datagram_type is_reliable) override;

    void set_receive_buffer_size(size_t size) override {
        // Do nothing.
    }
    void set_child_receive_buffer_size(size_t size) override {
        // Do nothing.
    }

    void dump() override;
};

} // sss namespace
