//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2015, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include "packet_frame.h"
#include "frame_format.h"
#include "sss/forward_ptrs.h"
#include <memory>
#include <boost/asio/buffers.hpp>

namespace sss {
namespace framing {

/**
 * framed packet
 * ⤷ IP header
 * ⤷ UDP header
 * ⤷ unencrypted packet header
 * ⤷ packet header
 * ⤷ collection of frames as (offset, size) pairs (const_buffers) into the packet buffer
*/
class framed_packet
{
    asio::mutable_buffer packet;              // whole packet
    asio::mutable_buffer unencrypted_header;  // packet subrange covering unencrypted header
    asio::mutable_buffer packet_header;       // packet subrange covering packet header
    std::vector<asio::mutable_buffer> frames; // packet subranges covering sequence of frames
};

/**
 * Given multiple packets to send and a packet buffer, figure out most efficient packing,
 * complying to security policy etc. and write those packets into the buffer.
 * Written packets are cleared from the queue (and inserted into the wait queue for ack if needed).
 * Prepared buffer is forwarded to channel layer for encryption.
 *
 * Channel and stream layers submit data to send to framing, which then assembles packets
 * using priority rules.
 *
 * Framing owns client's data not yet prepared for sending.
 * Assembled packets are owned by channel transmission layer.
 * When channel is shut down these frames must be returned to owning stream.
 * (They should be stream-buffered until acked)
 * ^- @todo this means they're better owned by the stream
 */
class framing_t
{
public:
    framing_t(channel_ptr c);

    void enframe(asio::mutable_buffer output);
    void deframe(asio::const_buffer input);

private:
    template <typename T>
    read_handler(asio::const_buffer input);

private:
    using read_handler_type = void (framing::*)(asio::const_buffer);
    std::array<read_handler_type, max_frame_count_t::value> handlers_;

    // Reference to channel associated with this framing instance.
    // When parsing received frames, obtain streams from channel by lsid/usid and call rx_*()
    // functions to process associated frames. Call channel's rx_*() functions to process
    // channel-level frames.
    channel_ptr channel_;
};

}

}
