//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "base_stream.h"

namespace ssu {

base_stream::base_stream(std::shared_ptr<host> h, 
                         const peer_id& peer,
                         std::shared_ptr<base_stream> parent)
    : abstract_stream(h)
{
    logger::debug() << "Constructing internal stream for peer " << peer;
}

base_stream::~base_stream()
{
    logger::debug() << "Destructing internal stream";
}

//txenqflow()
void base_stream::tx_enqueue_channel(bool tx_immediately)
{
    if (!attached())
        return tx_attach();

    // stream_channel* channel = tx_current_attachment->channel;
    // assert(channel && channel->is_active());

    // if (!tx_enqueued_channel)
    // {
    //     if (tx_queue.empty())
    //     {
    //         if (owner) {
    //             owner->ready_write();
    //         }
    //     }
    //     else
    //     {
    //         channel->enqueue_stream(this);
    //         tx_enqueued_channel = true;
    //     }
    // }

    // if (tx_immediately && channel->may_transmit())
    //     channel->ready_transmit();
}

bool base_stream::attached()
{
    return false;
}

void base_stream::tx_attach()
{}

//calcReceiveWindow
void base_stream::recalculate_receive_window()
{}

//calc
void base_stream::recalculate_transmit_window()
{}

void base_stream::connect_to(std::string const& service, std::string const& protocol)
{
    logger::debug() << "Connecting internal stream to " << service << ":" << protocol;
}

size_t base_stream::bytes_available() const
{
    return 0;
}

bool base_stream::at_end() const
{
    return true;
}

ssize_t base_stream::read_data(char* data, size_t max_size)
{
    return 0;
}

int base_stream::pending_records() const
{
    return 0;
}

ssize_t base_stream::read_record(char* data, size_t max_size)
{
    return 0;
}

byte_array base_stream::read_record(size_t max_size)
{
    return byte_array();
}

ssize_t base_stream::write_data(const char* data, size_t size, uint8_t endflags)
{
    return 0;
}

ssize_t base_stream::read_datagram(char* data, size_t max_size)
{
    return 0;
}

ssize_t base_stream::write_datagram(const char* data, size_t size, stream::datagram_type is_reliable)
{
    return 0;
}

byte_array base_stream::read_datagram(size_t max_size)
{
    return byte_array();
}

abstract_stream* base_stream:: open_substream()
{
    return 0;
}

abstract_stream* base_stream:: accept_substream()
{
    return 0;
}

bool base_stream::is_link_up() const
{
    return false;
}

void base_stream::shutdown(stream::shutdown_mode mode)
{
    logger::debug() << "Shutting down internal stream";
}

void base_stream::set_receive_buffer_size(size_t size)
{
    logger::debug() << "Setting internal stream receive buffer size " << size << " bytes";
}

void base_stream::set_child_receive_buffer_size(size_t size)
{
    logger::debug() << "Setting internal stream child receive buffer size " << size << " bytes";
}

void base_stream::dump()
{}


}
