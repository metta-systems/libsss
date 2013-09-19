//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "channel.h"
#include "logging.h"
#include "host.h"

namespace ssu {

constexpr int channel::header_len;
constexpr uint64_t channel::max_packet_sequence;

channel::channel(std::shared_ptr<host> host)
    : link_channel()
    , host_(host)
    , retransmit_timer_(host.get())
{
    // Initialize transmit congestion control state
    tx_events_.push(transmit_event_t(0, false));
    assert(tx_events_.size() == 1);
    mark_time_ = host_->current_time();
}

channel::~channel()
{}

void channel::start(bool initiate)
{
    logger::debug() << "channel: start " << (initiate ? "(initiator)" : "(responder)");

    assert(armor_);

    super::start(initiate);

    on_ready_transmit();

    set_link_status(link::status::up);
}

void channel::stop()
{
    logger::debug() << "channel: stop";
}

int channel::may_transmit()
{
    logger::debug() << "channel: may_transmit";
    return 1;
}

uint32_t channel::make_first_header_word(channel_number channel, uint32_t tx_sequence)
{
    constexpr uint32_t seq_bits = 24;  
    constexpr uint32_t seq_mask = (1 << seq_bits) - 1;

    // 31-24: channel number
    // 23-0: tx sequence number
    return (tx_sequence & seq_mask) | ((uint32_t)channel << seq_bits);
}

uint32_t channel::make_second_header_word(uint8_t ack_count, uint32_t ack_sequence)
{
    constexpr uint32_t ack_cnt_bits = 4; 
    constexpr uint32_t ack_cnt_mask = (1 << ack_cnt_bits) - 1;
    constexpr uint32_t ack_seq_bits = 24;   
    constexpr uint32_t ack_seq_mask = (1 << ack_seq_bits) - 1;

    // 31-28: reserved field
    // 27-24: ack count
    // 23-0: ack sequence number
    return (ack_sequence & ack_seq_mask) | ((uint32_t)ack_count & ack_cnt_mask) << ack_seq_bits;
}

bool channel::channel_transmit(byte_array& packet, uint64_t& packet_seq)
{
    // Include implicit acknowledgment of the latest packet(s) we've acked
    uint32_t ackseq = make_second_header_word(rx_ack_count_, rx_ack_sequence_);

    // Send the packet
    bool success = transmit(packet, ackseq, packet_seq, true);
    return success;
}

bool channel::transmit(byte_array& packet, uint32_t ack_seq, uint64_t& packet_seq, bool is_data)
{
    assert(is_active());

    logger::debug() << "Channel sending a packet";

    // Don't allow tx_sequence_ counter to wrap (@fixme re-key before it does!)
    packet_seq = tx_sequence_;
    assert(tx_sequence_ < max_packet_sequence);
    uint32_t ptxseq = make_first_header_word(remote_channel(), packet_seq);

    // Fill in the transmit and ACK sequence number fields.
    assert(packet.size() >= header_len);
    big_uint32_t* pkt_header = reinterpret_cast<big_uint32_t*>(packet.data());
    pkt_header[0] = ptxseq;
    pkt_header[1] = ack_seq;

    // Encrypt and compute the MAC for the packet
    byte_array epkt = armor_->transmit_encode(tx_sequence_, packet);

    // Bump transmit sequence number,
    // and timestamp if this packet is marked for RTT measurement
    // This is the "Point of no return" -
    // a failure after this still consumes sequence number space.
    if (tx_sequence_ == mark_sequence_) {
        mark_time_ = host_->current_time();
        mark_acks_ = 0;
        mark_base_ = tx_ack_sequence_;
        mark_sent_ = tx_sequence_ - tx_ack_sequence_;
    }
    tx_sequence_++;

    // Record the transmission event
    transmit_event_t evt(packet.size(), is_data);
    if (is_data)
    {
        tx_inflight_count_++;
        tx_inflight_size_ += evt.size_;
    }
    tx_events_.push(evt);
    assert(tx_event_sequence_ + tx_events_.size() == tx_sequence_);
    assert(tx_inflight_count_ <= (unsigned)tx_events_.size());

    // Ship it out
    return send(epkt);
}

bool channel::transmit_ack(byte_array &packet, uint64_t ackseq, unsigned ackct)
{
    logger::debug() << "channel: transmit_ack";
    return false;
}

void channel::acknowledged(uint64_t txseq, int npackets, uint64_t rxackseq)
{
    logger::debug() << "channel: acknowledged";
}

void channel::missed(uint64_t txseq, int npackets)
{
    logger::debug() << "channel: missed";
}

void channel::expire(uint64_t txseq, int npackets)
{
    logger::debug() << "channel: expire";
}

void channel::receive(const byte_array& msg, const link_endpoint& src)
{
    logger::debug() << "channel: receive";
}

} // ssu namespace
