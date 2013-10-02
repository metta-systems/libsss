//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "datagram_stream.h"
#include "logging.h"

using namespace std;

namespace ssu {

//=================================================================================================
// datagram_stream
//=================================================================================================

void datagram_stream::shutdown(stream::shutdown_mode mode)
{}

int datagram_stream::pending_records() const
{
    return 0;
}

ssize_t datagram_stream::pending_record_size() const
{
    return 0;
}

ssize_t datagram_stream::read_record(char* data, ssize_t max_size)
{
    return -1;
}

byte_array datagram_stream::read_record(ssize_t max_size)
{
    return byte_array();
}

ssize_t datagram_stream::read_data(char* data, ssize_t max_size)
{
    return -1;
}

ssize_t datagram_stream::write_data(const char* data, ssize_t size, uint8_t endflags)
{
    return -1;
}

abstract_stream* datagram_stream::open_substream()
{
    return nullptr;
}

abstract_stream* datagram_stream::accept_substream()
{
    return nullptr;
}

ssize_t datagram_stream::read_datagram(char* data, ssize_t max_size)
{
    return -1;
}

byte_array datagram_stream::read_datagram(ssize_t max_size)
{
    return byte_array();
}

ssize_t datagram_stream::write_datagram(const char* data, ssize_t size, stream::datagram_type is_reliable)
{
    return -1;
}

void datagram_stream::dump()
{}

} // ssu namespace
