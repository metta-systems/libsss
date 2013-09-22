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
#include "timer.h"
#include "make_unique.h"

namespace ssu {

//=================================================================================================
// channel private_data implementation
//=================================================================================================

struct transmit_event_t
{
    int32_t size_;   ///< Total size of packet including hdr
    bool    data_;   ///< Was an upper-layer data packet
    bool    pipe_;   ///< Currently counted toward transmit_data_pipe

    inline transmit_event_t(int32_t size, bool is_data)
        : size_(size)
        , data_(is_data)
        , pipe_(is_data)
    {
        logger::debug() << "New transmission event for " << size_ << (data_ ? " data bytes" : " control bytes");
    }
};

class channel::private_data
{
public:
    std::shared_ptr<host>          host_;

    //-------------------------------------------
    // Transmit state
    //-------------------------------------------

    /// Next sequence number to transmit.
    packet_seq_t tx_sequence_{1};
    /// Record of transmission events (XX data sizes).
    std::queue<transmit_event_t> tx_events_;
    /// Seqno of oldest recorded tx event.
    packet_seq_t tx_event_sequence_{0};
    /// Highest transmit sequence number ACK'd.
    packet_seq_t tx_ack_sequence_{0};
    // uint64_t recovseq;   ///< Sequence at which fast recovery finishes
    /// Transmit sequence number of "marked" packet.
    packet_seq_t mark_sequence_{1};
    /// Snapshot of tx_ack_sequence_ at time mark was placed.
    packet_seq_t mark_base_{0};
    /// Time at which marked packet was sent.
    boost::posix_time::ptime mark_time_;
    // uint32_t txackmask;  ///< Mask of packets transmitted and ACK'd
    /// Data packets currently in flight.
    uint32_t tx_inflight_count_{0};
    /// Data bytes currently in flight.
    uint32_t tx_inflight_size_{0};
    /// Number of ACK'd packets since last mark.
    uint32_t mark_acks_{0};
    /// Number of ACKs expected after last mark.
    uint32_t mark_sent_{0};
    // uint32_t cwnd;       ///< Current congestion window
    // bool cwndlim;       ///< We were cwnd-limited this round-trip

    // Retransmit state
    async::timer retransmit_timer_;  ///< Retransmit timer.

    //-------------------------------------------
    // Receive state
    //-------------------------------------------

    /// Highest sequence number received so far.
    packet_seq_t rx_sequence_{0};
    // quint32 rxmask;     ///< Mask of packets received so far

    // Receive-side ACK state
    /// Highest sequence number acknowledged so far.
    packet_seq_t rx_ack_sequence_{0};
    // //quint32 rxackmask;  // Mask of packets received & acknowledged
    /// Number of contiguous packets received before rx_ack_sequence_.
    packet_seq_t rx_ack_count_{0};
    /// Number of contiguous packets not yet ACKed.
    uint8_t rx_unacked_{0};
    // bool delayack;      ///< Enable delayed acknowledgments
    // Timer acktimer;     ///< Delayed ACK timer

    //-------------------------------------------
    // Channel statistics.
    //-------------------------------------------

    async::timer::duration_type cumulative_rtt_;

public:
    private_data(std::shared_ptr<host> host)
        : host_(host)
        , retransmit_timer_(host.get())
    {}
};


//=================================================================================================
// channel
//=================================================================================================

constexpr int channel::header_len;
constexpr packet_seq_t channel::max_packet_sequence;

channel::channel(std::shared_ptr<host> host)
    : link_channel()
    , pimpl_(make_unique<private_data>(host))
{
    // Initialize transmit congestion control state
    pimpl_->tx_events_.push(transmit_event_t(0, false));
    assert(pimpl_->tx_events_.size() == 1);
    pimpl_->mark_time_ = host->current_time();

    reset_congestion_control();
}

channel::~channel()
{}

std::shared_ptr<host> channel::get_host()
{
    return pimpl_->host_;
}

void channel::start(bool initiate)
{
    logger::debug() << "channel: start " << (initiate ? "(initiator)" : "(responder)");

    assert(armor_);

    super::start(initiate);

    // We're ready to go!
    start_retransmit_timer();
    on_ready_transmit();

    set_link_status(link::status::up);
}

void channel::stop()
{
    logger::debug() << "channel: stop";
}

void channel::reset_congestion_control()
{
    pimpl_->cumulative_rtt_ = boost::posix_time::milliseconds(500);
}

void channel::start_retransmit_timer()
{
    async::timer::duration_type timeout =
        boost::posix_time::milliseconds(pimpl_->cumulative_rtt_.total_milliseconds() * 2);
    pimpl_->retransmit_timer_.start(timeout); // Wait for full round-trip time.
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
    assert(packet.size() > header_len); // Must be non-empty packet.

    // Include implicit acknowledgment of the latest packet(s) we've acked
    uint32_t ackseq = make_second_header_word(pimpl_->rx_ack_count_, pimpl_->rx_ack_sequence_);

    // Send the packet
    bool success = transmit(packet, ackseq, packet_seq, true);

    // If the retransmission timer is inactive, start it afresh.
    // (If this was a retransmission, rtxTimeout() would have restarted it.)
    if (!pimpl_->retransmit_timer_.is_active()) {
        start_retransmit_timer();
    }

    return success;
}

bool channel::transmit(byte_array& packet, uint32_t ack_seq, uint64_t& packet_seq, bool is_data)
{
    assert(is_active());

    logger::debug() << "Channel sending a packet";

    // Don't allow tx_sequence_ counter to wrap (@fixme re-key before it does!)
    packet_seq = pimpl_->tx_sequence_;
    assert(pimpl_->tx_sequence_ < max_packet_sequence);
    uint32_t ptxseq = make_first_header_word(remote_channel(), packet_seq);

    // Fill in the transmit and ACK sequence number fields.
    assert(packet.size() >= header_len);
    big_uint32_t* pkt_header = reinterpret_cast<big_uint32_t*>(packet.data());
    pkt_header[0] = ptxseq;
    pkt_header[1] = ack_seq;

    // Encrypt and compute the MAC for the packet
    byte_array epkt = armor_->transmit_encode(pimpl_->tx_sequence_, packet);

    // Bump transmit sequence number,
    // and timestamp if this packet is marked for RTT measurement
    // This is the "Point of no return" -
    // a failure after this still consumes sequence number space.
    if (pimpl_->tx_sequence_ == pimpl_->mark_sequence_) {
        pimpl_->mark_time_ = pimpl_->host_->current_time();
        pimpl_->mark_acks_ = 0;
        pimpl_->mark_base_ = pimpl_->tx_ack_sequence_;
        pimpl_->mark_sent_ = pimpl_->tx_sequence_ - pimpl_->tx_ack_sequence_;
    }
    pimpl_->tx_sequence_++;

    // Record the transmission event
    transmit_event_t evt(packet.size(), is_data);
    if (is_data)
    {
        pimpl_->tx_inflight_count_++;
        pimpl_->tx_inflight_size_ += evt.size_;
    }
    pimpl_->tx_events_.push(evt);
    assert(pimpl_->tx_event_sequence_ + pimpl_->tx_events_.size() == pimpl_->tx_sequence_);
    assert(pimpl_->tx_inflight_count_ <= (unsigned)pimpl_->tx_events_.size());

    // Ship it out
    return send(epkt);
}

void channel::acknowledge(uint16_t pktseq, bool send_ack)
{
    logger::debug() << "channel: acknowledge UNIMPLEMENTED " << pktseq << (send_ack ? " sending" : " not sending");
}

bool channel::transmit_ack(byte_array &packet, uint64_t ackseq, unsigned ackct)
{
    logger::debug() << "channel: transmit_ack UNIMPLEMENTED";
    return false;
}

void channel::acknowledged(uint64_t txseq, int npackets, uint64_t rxackseq)
{
    logger::debug() << "channel: acknowledged UNIMPLEMENTED";
}

void channel::missed(uint64_t txseq, int npackets)
{
    logger::debug() << "channel: missed UNIMPLEMENTED";
}

void channel::expire(uint64_t txseq, int npackets)
{
    logger::debug() << "channel: expire UNIMPLEMENTED";
}

void channel::receive(const byte_array& msg, const link_endpoint& src)
{
    // Determine the full 64-bit packet sequence number
    big_uint32_t ptxseq = msg.as<big_uint32_t>()[0];

    channel_number pktchan = ptxseq >> 24;
    assert(pktchan == local_channel());    // enforced by sock.cc

    int32_t seqdiff = ((int32_t)(ptxseq << 8)
                    - ((int32_t)pimpl_->rx_sequence_ << 8))
                    >> 8;

    packet_seq_t pktseq = pimpl_->rx_sequence_ + seqdiff;
    logger::debug() << "channel: receive - rxseq " << pktseq << ", size " << msg.size();

    byte_array pkt = msg;
    // Authenticate and decrypt the packet
    if (!armor_->receive_decode(pktseq, pkt)) {
        logger::warning() << "Received packet auth failed on rx " << pktseq;
        return;
    }
    //@todo replace first word in pkt with data from msg - it's unencrypted

    if (channel_receive(pktseq, pkt))
        acknowledge(pktseq, true);
        // XX should still replay-protect even if no ack!
}

} // ssu namespace
