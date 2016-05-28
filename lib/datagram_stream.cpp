//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include <boost/log/trivial.hpp>
#include "sss/streams/datagram_stream.h"

using namespace std;

namespace sss {

//=================================================================================================
// datagram_stream
//=================================================================================================

void datagram_stream::shutdown(stream::shutdown_mode mode)
{
    BOOST_LOG_TRIVIAL(debug) << "Shutting down datagram stream " << this;
    if (mode != stream::shutdown_mode::write) {
        pos_ = size();
    }

    // We hold a shared_ptr<> to datagram_stream, so it will get deleted once client let it go.
}

ssize_t datagram_stream::read_record(char* data, ssize_t max_size)
{
    // A datagram can contain only one message by definition.
    // So read whatever of it the caller wants and discard the rest.
    int actual_size = datagram_stream::read_data(data, max_size);
    pos_ = size();
    return actual_size;
}

byte_array datagram_stream::read_record(ssize_t max_size)
{
    if (pos_ == 0 and max_size >= size())
    {
        // The quick and easy case...
        pos_ = size();
        return payload_;
    }

    ssize_t actual_size = min(remain(), max_size);
    byte_array buf(payload_.mid(pos_, actual_size));
    pos_ += actual_size;
    return buf;
}

ssize_t datagram_stream::read_data(char* data, ssize_t max_size)
{
    ssize_t actual_size = min(remain(), max_size);
    memcpy(data, payload_.const_data() + pos_, actual_size);
    pos_ += actual_size;
    return actual_size;
}

ssize_t datagram_stream::write_data(const char* data, ssize_t size, uint8_t endflags)
{
    set_error("Can't write to ephemeral datagram-streams");
    return -1;
}

shared_ptr<abstract_stream> datagram_stream::open_substream()
{
    set_error("Ephemeral datagram-streams cannot have substreams");
    return nullptr;
}

shared_ptr<abstract_stream> datagram_stream::accept_substream()
{
    set_error("Ephemeral datagram-streams cannot have substreams");
    return nullptr;
}

ssize_t datagram_stream::read_datagram(char* data, ssize_t max_size)
{
    set_error("Ephemeral datagram-streams cannot have sub-datagrams");
    return -1;
}

byte_array datagram_stream::read_datagram(ssize_t max_size)
{
    set_error("Ephemeral datagram-streams cannot have sub-datagrams");
    return byte_array();
}

ssize_t datagram_stream::write_datagram(const char* data, ssize_t size, stream::datagram_type is_reliable)
{
    set_error("Ephemeral datagram-streams cannot have sub-datagrams");
    return -1;
}

void datagram_stream::dump()
{
    BOOST_LOG_TRIVIAL(debug) << this << " datagram_stream - size " << size() << ", pos " << pos_;
}

} // sss namespace
