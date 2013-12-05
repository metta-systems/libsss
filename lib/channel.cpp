//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include <deque>
#include <boost/format.hpp>
#include "channel.h"
#include "logging.h"
#include "host.h"
#include "timer.h"
#include "make_unique.h"

using namespace std;
namespace bp = boost::posix_time;

namespace ssu {

//=================================================================================================
// channel private_data implementation
//=================================================================================================

/// Size of rxmask, rxackmask, and txackmask fields in bits
static constexpr int maskBits = 32;

static constexpr int max_ack_count = 0xf;

static constexpr unsigned CWND_MIN = 2;     // Min congestion window (packets/RTT)
static constexpr unsigned CWND_MAX = 1<<20; // Max congestion window (packets/RTT)

static const async::timer::duration_type RTT_INIT = bp::milliseconds(500);
static const async::timer::duration_type RTT_MAX = bp::seconds(30);

constexpr int channel::header_len;
constexpr packet_seq_t channel::max_packet_sequence;

struct transmit_event_t
{
    int32_t size_;   ///< Total size of packet including header
    bool    data_;   ///< Was an upper-layer data packet
    bool    pipe_;   ///< Currently counted toward transmit_data_pipe

    inline transmit_event_t(int32_t size, bool is_data)
        : size_(size)
        , data_(is_data)
        , pipe_(is_data)
    {
        logger::debug() << "New transmission event for " << size_
            << (data_ ? " data bytes" : " control bytes");
    }
};

/**
 * State shared between channel and congestion control.
 */
class shared_state
{
    shared_ptr<host> const& host_;

public:
    //-------------------------------------------
    /** @name Transmit state */
    //-------------------------------------------
    /**@{*/

    /// Next sequence number to transmit.
    packet_seq_t tx_sequence_{1};
    /// Record of transmission events (XX data sizes).
    deque<transmit_event_t> tx_events_;
    /// Seqno of oldest recorded tx event.
    packet_seq_t tx_event_sequence_{0};
    /// Highest transmit sequence number ACK'd.
    packet_seq_t tx_ack_sequence_{0};
    /// Transmit sequence number of "marked" packet.
    packet_seq_t mark_sequence_{1};
    /// Snapshot of tx_ack_sequence_ at time mark was placed.
    packet_seq_t mark_base_{0};
    /// Time at which marked packet was sent.
    bp::ptime mark_time_;
    /// Mask of packets transmitted and ACK'd (fictitious packet 0 already received)
    uint32_t tx_ack_mask_{1};
    /// Data packets currently in flight.
    uint32_t tx_inflight_count_{0};
    /// Data bytes currently in flight.
    uint32_t tx_inflight_size_{0};
    /// Number of ACK'd packets since last mark.
    uint32_t mark_acks_{0};
    /// Number of ACKs expected after last mark.
    uint32_t mark_sent_{0};

    /**@}*/
    //-------------------------------------------
    /** @name Receive state */
    //-------------------------------------------
    /**@{*/

    /// Highest sequence number received so far.
    packet_seq_t rx_sequence_{0};
    /// Mask of packets received so far (1 = fictitious packet 0)
    uint32_t rx_mask_{1};

    // Receive-side ACK state
    /// Highest sequence number acknowledged so far.
    packet_seq_t rx_ack_sequence_{0};
    // //quint32 rxackmask;  // Mask of packets received & acknowledged
    /// Number of contiguous packets received before rx_ack_sequence_.
    int rx_ack_count_{0};
    /// Number of contiguous packets not yet ACKed.
    uint8_t rx_unacked_{0};
    unsigned miss_threshold_{3}; ///< Threshold at which to infer packets dropped
    // @todo make adaptive for robustness to reordering

    /**@}*/

    shared_state(shared_ptr<host> const& host)
        : host_(host)
        , mark_time_(host->current_time())
    {}

    /// Compute the time elapsed since the mark.
    inline async::timer::duration_type
    elapsed_since_mark()
    {
        return host_->current_time() - mark_time_;
    }
};

/**
 * Congestion Control modes
 */
enum CCMode {
    CC_TCP,
    CC_AGGRESSIVE,
    CC_DELAY,
    CC_VEGAS,
    CC_CTCP,
    CC_FIXED,
};

/**
 * Channel's congestion control strategy.
 */
class congestion_control_strategy
{
public:
    CCMode mode; ///< Congestion control method
    shared_ptr<shared_state> const& state_;

    uint32_t cwnd{CWND_MIN};       ///< Current congestion window
    bool cwnd_limited_{true};      ///< We were cwnd-limited this round-trip

    //-------------------------------------------
    /** @name Transmit state */
    //-------------------------------------------
    /**@{*/

    /// Sequence at which fast recovery finishes.
    packet_seq_t recovseq;

    // TCP congestion control
    uint32_t ssthresh;   ///< Slow start threshold
    bool sstoggle;      ///< Slow start toggle flag for CC_VEGAS

    // Aggressive congestion control
    uint32_t ssbase;     ///< Slow start baseline

    // Low-delay congestion control
    int cwndinc;
    async::timer::duration_type lastrtt;        ///< Measured RTT of last round-trip
    float lastpps;      ///< Measured PPS of last round-trip
    uint32_t basewnd;
    float basertt, basepps, basepwr;

    // TCP Vegas-like congestion control
    float cwndmax;

    /**@}*/
    //-------------------------------------------
    /** @name Channel statistics */
    //-------------------------------------------
    /**@{*/

    /// Cumulative measured RTT in milliseconds.
    async::timer::duration_type cumulative_rtt_;
    float cumulative_rtt_variance_;    ///< Cumulative variation in RTT
    float cumulative_pps_;       ///< Cumulative measured packets per second
    float cumulative_pps_var;    ///< Cumulative variation in PPS
    float cumpwr;       ///< Cumulative measured network power (pps/rtt)
    float cumbps;       ///< Cumulative measured bytes per second
    float cumloss;      ///< Cumulative measured packet loss ratio

    /**@}*/

    congestion_control_strategy(shared_ptr<shared_state> const& state)
        : state_(state)
    {}

    /// Reset congestion control.
    void reset();
    /// Update congestion control on missed packet ack.
    void missed(uint64_t pktseq);
    /// Update on expired packet.
    void timeout();
    /// Update on newly received acks.
    void update(unsigned new_packets);
    /// Update rtt information.
    void rtt_update(float pps, float rtt);
    /// Update rtt cumulative statistics.
    void stats_update(float& pps_out, float& rtt_out);
};

void congestion_control_strategy::reset()
{
    logger::debug() << "cc reset: mode " << mode;
    cwnd = CWND_MIN;
    cwnd_limited_ = true;
    ssthresh = CWND_MAX;
    sstoggle = true;
    ssbase = 0;
    cwndinc = 1;
    cwndmax = CWND_MIN;
    lastrtt = bp::milliseconds(0);
    lastpps = 0;
    basertt = 0;
    basepps = 0;
    cumulative_rtt_ = RTT_INIT;
    cumulative_rtt_variance_ = 0;
    cumulative_pps_ = 0;
    cumulative_pps_var = 0;
    cumpwr = 0;
    cumbps = 0;
    cumloss = 0;
}

void congestion_control_strategy::missed(uint64_t pktseq)
{
    logger::debug() << "Missed seq " << pktseq;

    switch (mode)
    {
        case CC_TCP: 
        case CC_DELAY:
        case CC_VEGAS:
            // Packet loss detected -
            // perform standard TCP congestion control
            if (pktseq <= recovseq) {
                // We're in a fast recovery window:
                // this isn't a new loss event.
                break;
            }

            // new loss event: cut ssthresh and cwnd
            //ssthresh = (tx_sequence_ - tx_ack_sequence_) / 2;    XXX
            ssthresh = cwnd / 2;
            ssthresh = max(ssthresh, CWND_MIN);
            // logger::debug() << "%d PACKETS LOST: cwnd %d -> %d", ackdiff - newpackets, cwnd, ssthresh);
            cwnd = ssthresh;

            // fast recovery for the rest of this window
            recovseq = state_->tx_sequence_;

            break;

        case CC_AGGRESSIVE: {
            // Number of packets we think have been lost
            // so far during this round-trip.
            int lost = (state_->tx_ack_sequence_ - state_->mark_base_) - state_->mark_acks_;
            lost = max(0, lost);

            // Number of packets we expect to receive,
            // assuming the lost ones are really lost
            // and we don't lose any more this round-trip.
            unsigned expected = state_->mark_sent_ - lost;

            // Clamp the congestion window to this value.
            if (expected < cwnd) {
                logger::debug() << "PACKETS LOST: cwnd " << cwnd << "->" << expected;
                cwnd = ssbase = expected;
                cwnd = max(CWND_MIN, cwnd);
            }
            break;
        }

        case CC_CTCP:
            assert(0);    // XXX

        case CC_FIXED:
            break;  // fixed cwnd, no congestion control
    }
}

void congestion_control_strategy::timeout()
{
    // If fixed cwnd, no congestion control, otherwise 
    // Reset cwnd and go back to slow start
    if (mode != CC_FIXED)
    {
        ssthresh = state_->tx_inflight_count_ / 2;
        ssthresh = max(ssthresh, CWND_MIN);
        cwnd = CWND_MIN;
        logger::debug() << "rtxTimeout: ssthresh=" << ssthresh << ", cwnd=" << cwnd;
    }
}

void congestion_control_strategy::update(unsigned new_packets)
{
    switch (mode)
    {
        case CC_VEGAS:
            sstoggle = !sstoggle;
            if (sstoggle)
                break;  // do slow start only once every two RTTs

            // fall through...
        case CC_TCP: {
            // During standard TCP slow start procedure,
            // increment cwnd for each newly-ACKed packet.
            // XX TCP spec allows this to be <=,
            // which puts us in slow start briefly after each loss...
            if (new_packets and cwnd_limited_ and cwnd < ssthresh)
            {
                cwnd = min(cwnd + new_packets, ssthresh);
                logger::debug() << "Slow start: " << new_packets << " new ACKs; boost cwnd to "
                    << cwnd << " (ssthresh " << ssthresh << ")";
            }
            break; }

        case CC_DELAY:
            if (cwndinc < 0)    // Only slow start during up-phase
                break;

            // fall through...
        case CC_AGGRESSIVE: {
            // We're always in slow start, but we only count ACKs received
            // on schedule and after a per-roundtrip baseline.
            if (state_->mark_acks_ > ssbase and state_->elapsed_since_mark() <= lastrtt) {
                cwnd += min(new_packets, state_->mark_acks_ - ssbase);
                logger::debug() << "Slow start: " << new_packets
                    << " new ACKs; boost cwnd to " << cwnd;
            }
            break; }

        case CC_CTCP:
            assert(0);    // XXX

        case CC_FIXED:
            break;  // fixed cwnd, no congestion control
    }
}

void congestion_control_strategy::rtt_update(float pps, float rtt)
{
    switch (mode)
    {
        case CC_TCP:
            // Normal TCP congestion control: during congestion avoidance,
            // increment cwnd once each RTT, but only on round-trips that were cwnd-limited.
            if (cwnd_limited_) {
                cwnd++;
                logger::debug() << "cwnd " << cwnd << " ssthresh " << ssthresh;
            }
            cwnd_limited_ = false;
            break;

        case CC_AGGRESSIVE:
            break;

        case CC_DELAY: {
            float pwr = pps / rtt;
            if (pwr > basepwr) {
                basepwr = pwr;
                basertt = rtt;
                basepps = pps;
                basewnd = state_->mark_acks_;
            } else if (state_->mark_acks_ <= basewnd && rtt > basertt) {
                basertt = rtt;
                basepwr = basepps / basertt;
            } else if (state_->mark_acks_ >= basewnd && pps < basepps) {
                basepps = pps;
                basepwr = basepps / basertt;
            }

            if (cwndinc > 0) {
                // Window going up.
                // If RTT makes a significant jump, reverse.
                if (rtt > basertt || cwnd >= CWND_MAX) {
                    cwndinc = -1;
                } else {
                    // Additively increase the window
                    cwnd += cwndinc;
                }
            } else {
                // Window going down.
                // If PPS makes a significant dive, reverse.
                if (pps < basepps || cwnd <= CWND_MIN) {
                    ssbase = cwnd++;
                    cwndinc = +1;
                } else {
                    // Additively decrease the window
                    cwnd += cwndinc;
                }
            }
            cwnd = max(CWND_MIN, cwnd);
            cwnd = min(CWND_MAX, cwnd);

            logger::debug() << boost::format(
                "RT: pwr %.0f[%.0f/%.0f]@%d base %.0f[%.0f/%.0f]@%d cwnd %d%+d")
                % (pwr*1000.0)
                % pps
                % rtt
                % state_->mark_acks_
                % (basepwr*1000.0)
                % basepps
                % basertt
                % basewnd
                % cwnd
                % cwndinc;
            break;
        }

        case CC_VEGAS: {
            // Keep track of the lowest RTT ever seen,
            // as per the original Vegas algorithm.
            // This has the known problem that it screws up
            // if the path's actual base RTT changes.
            if (basertt == 0)   // first packet
                basertt = rtt;
            else if (rtt < basertt)
                basertt = rtt;
            //else
            //  basertt = (basertt * 255.0 + rtt) / 256.0;

            float expect = (float)state_->mark_sent_ / basertt;
            float actual = (float)state_->mark_sent_ / rtt;
            float diffpps = expect - actual;
            assert(diffpps >= 0.0);
            float diffpprt = diffpps * rtt;

            if (diffpprt < 1.0 && cwnd < CWND_MAX && cwnd_limited_) {
                cwnd++;
                // ssthresh = max(ssthresh, cwnd / 2); ??
            } else if (diffpprt > 3.0 && cwnd > CWND_MIN) {
                cwnd--;
                ssthresh = min(ssthresh, cwnd); // /2??
            }

            logger::debug() << boost::format("Round-trip: win %d basertt %.3f rtt %d "
                "exp-pps %f act-pps %f diff-pprt %.3f cwnd %d")
                % state_->mark_sent_
                % basertt
                % rtt
                % (expect*1000000.0)
                % (actual*1000000.0)
                % diffpprt
                % cwnd;
            break;
        }

        case CC_CTCP: {
#if 0
            k = 0.8; a = 1/8; B = 1/2
            if (in-recovery)
                ...
            else if (diff < y) {
                dwnd += sqrt(win)/8.0 - 1;
            } else
                dwnd -= C * diff;
#endif
            break; }

        case CC_FIXED:
            break;  // fixed cwnd, no congestion control
    }
}

void congestion_control_strategy::stats_update(float& pps_out, float& rtt_out)
{
    // 'rtt' is the total round-trip delay in microseconds before
    // we receive an ACK for a packet at or beyond the mark.
    // Fold this into 'rtt' to determine avg round-trip time,
    // and restart the timer to measure the next round-trip.
    async::timer::duration_type rtt = state_->elapsed_since_mark();
    rtt = max(bp::time_duration(bp::microseconds(1)), min(RTT_MAX, rtt));
    cumulative_rtt_ = bp::microseconds(
        (cumulative_rtt_.total_microseconds() * 7.0 + rtt.total_microseconds()) / 8.0);

    rtt_out = rtt.total_microseconds();

    // Compute an RTT variance measure
    float rttvar = fabsf((rtt - cumulative_rtt_).total_microseconds());
    cumulative_rtt_variance_ = ((cumulative_rtt_variance_ * 7.0) + rttvar) / 8.0;

    // 'mark_acks_' is the number of unique packets ACKed
    // by the receiver during the time since the last mark.
    // Use this to gauge throughput during this round-trip.
    float pps = (float)state_->mark_acks_ * 1000000.0 / rtt.total_microseconds();
    cumulative_pps_ = ((cumulative_pps_ * 7.0) + pps) / 8.0;

    pps_out = pps;

    // "Power" measures network efficiency
    // in the sense of both minimizing rtt and maximizing pps.
    float pwr = pps / rtt.total_microseconds();
    cumpwr = ((cumpwr * 7.0) + pwr) / 8.0;

    // Compute a PPS variance measure
    float ppsvar = fabsf(pps - cumulative_pps_);
    cumulative_pps_var = ((cumulative_pps_var * 7.0) + ppsvar) / 8.0;

    // Calculate loss rate during this last round-trip,
    // and a cumulative loss ratio.
    // Could go out of (0.0,1.0) range due to out-of-order acks.
    float loss = (float)(state_->mark_sent_ - state_->mark_acks_) / (float)state_->mark_sent_;
    loss = max(0.0f, min(1.0f, loss));
    cumloss = ((cumloss * 7.0) + loss) / 8.0;

    // Reset pimpl_->mark_sequence_ to be the next packet transmitted.
    // The new timestamp will be taken when that packet is sent.
    state_->mark_sequence_ = state_->tx_sequence_;

    lastrtt = rtt;
    lastpps = pps;
}

// public:
//     // Set the congestion controller for this channel.
//     // This must be set if the client wishes to call mayTransmit().
//     //inline void setCongestionController(FlowCC *cc) { this->cc = cc; }
//     //inline FlowCC *congestionController() { return cc; }

// public:
//     inline CCMode ccMode() const { return ccmode; }
//     inline void setCCMode(CCMode mode) { ccmode = mode; ccReset(); }

//     /// for CC_FIXED: fixed congestion window for reserved-bandwidth links
//     inline void setCCWindow(int cwnd) { this->cwnd = cwnd; }

//     /// Congestion information accessors for flow monitoring purposes
//     inline int txCongestionWindow() { return cwnd; }
//     inline int txBytesInFlight() { return txfltsize; }
//     inline int txPacketsInFlight() { return txfltcnt; }

// --end CC control--------------------------------------------------

class channel::private_data
{
public:
    shared_ptr<host>          host_;
    shared_ptr<shared_state>  state_;

    //-------------------------------------------
    // Congestion control
    //-------------------------------------------
    unique_ptr<congestion_control_strategy> congestion_control;
    bool nocc_{false};

    // bool delayack;      ///< Enable delayed acknowledgments
    async::timer ack_timer_;  ///< Delayed ACK timer.

    // Retransmit state
    async::timer retransmit_timer_;  ///< Retransmit timer.

    async::timer stats_timer_;

public:
    private_data(shared_ptr<host> host)
        : host_(host)
        , state_(make_shared<shared_state>(host_))
        , ack_timer_(host.get())
        , retransmit_timer_(host.get())
        , stats_timer_(host.get())
    {
        // Initialize transmit congestion control state
        state_->tx_events_.push_back(transmit_event_t(0, false));
        assert(state_->tx_events_.size() == 1);

        reset_congestion_control();
    }

    ~private_data() {
        logger::debug() << "~channel::private_data";
    }

    void cc_and_rtt_update(unsigned new_packets, packet_seq_t ackseq);

    void stats_timeout();

    void reset_congestion_control();

    /// Compute current number of transmitted but un-acknowledged packets.
    /// This count may include raw ACK packets, for which we expect no acknowledgments
    /// unless they happen to be piggybacked on data coming back.
    inline int64_t unacked_packets() { return state_->tx_sequence_ - state_->tx_ack_sequence_; }
};

// @todo Move this to cc_strategy implementation.
void channel::private_data::reset_congestion_control()
{
    congestion_control.reset();
    congestion_control = stdext::make_unique<congestion_control_strategy>(state_);

    // --CC control---------------------------------------------------
    congestion_control->mode = CC_TCP;
    // delayack = true;
    // static_assert(sizeof(txackmask)*8 == maskBits);

    // // Initialize transmit congestion control state
    // recovseq = 1;

    // Initialize congestion control state
    congestion_control->reset();

    // Statistics gathering state
    stats_timer_.on_timeout.connect([this](bool) {
        stats_timeout();
    });
    stats_timer_.start(bp::seconds(5));
}

// Transmit statistics
void channel::private_data::stats_timeout()
{
    logger::info() << boost::format("STATS: txseq %llu, txackseq %llu, rxseq %llu, rxackseq %llu, "
        "txfltcnt %d, cwnd %d, ssthresh %d, "
        "cumrtt %.3f, cumpps %.3f, cumloss %.3f")
        % state_->tx_sequence_
        % state_->tx_ack_sequence_
        % state_->rx_sequence_
        % state_->rx_ack_sequence_
        % state_->tx_inflight_count_
        % congestion_control->cwnd
        % congestion_control->ssthresh
        % congestion_control->cumulative_rtt_
        % congestion_control->cumulative_pps_
        % congestion_control->cumloss;
}

void channel::private_data::cc_and_rtt_update(unsigned new_packets, packet_seq_t ackseq)
{
    if (!nocc_) {
        congestion_control->update(new_packets);
    }

    // When ackseq passes mark_sequence_, we've observed a round-trip,
    // so update our round-trip statistics.
    if (ackseq >= state_->mark_sequence_)
    {
        float rtt, pps;
        congestion_control->stats_update(pps, rtt);

        if (!nocc_)
        {
            congestion_control->rtt_update(pps, rtt);

            logger::debug() << boost::format(
                "Cumulative: rtt %.3f[±%.3f] pps %.3f[±%.3f] pwr %.3f loss %.3f")
                % congestion_control->cumulative_rtt_
                % congestion_control->cumulative_rtt_variance_
                % congestion_control->cumulative_pps_
                % congestion_control->cumulative_pps_var
                % congestion_control->cumpwr
                % congestion_control->cumloss;
        }
        else
        {
            logger::debug() << "End-to-end rtt " << rtt
                << " cumulative rtt " << congestion_control->cumulative_rtt_;
        }
    }

    // Always clamp cwnd against CWND_MAX.
    congestion_control->cwnd = min(congestion_control->cwnd, CWND_MAX);
}

//=================================================================================================
// channel
//=================================================================================================

channel::channel(shared_ptr<host> host)
    : link_channel()
    , pimpl_(stdext::make_unique<private_data>(host))
{
    pimpl_->retransmit_timer_.on_timeout.connect([this](bool fail) {
        retransmit_timeout(fail);
    });

    // Delayed ACK state
    pimpl_->ack_timer_.on_timeout.connect([this](bool) {
        ack_timeout();
    });
}

channel::~channel()
{}

shared_ptr<host> channel::get_host()
{
    return pimpl_->host_;
}

void channel::start(bool initiate)
{
    logger::debug() << "channel: start " << (initiate ? "(initiator)" : "(responder)");

    assert(armor_);

    super::start(initiate);

    pimpl_->nocc_ = is_link_congestion_controlled();

    // We're ready to go!
    set_link_status(link::status::up);
    on_ready_transmit();
    start_retransmit_timer();
}

void channel::stop()
{
    logger::debug() << "channel: stop";
    pimpl_->retransmit_timer_.stop();
    pimpl_->ack_timer_.stop();
    pimpl_->stats_timer_.stop();

    super::stop();

    set_link_status(link::status::down);
}

int channel::may_transmit()
{
    logger::debug() << "channel: may_transmit";
    if (pimpl_->nocc_) {
        return super::may_transmit();
    }

    if (pimpl_->congestion_control->cwnd > pimpl_->state_->tx_inflight_count_) {
        int allowance = pimpl_->congestion_control->cwnd - pimpl_->state_->tx_inflight_count_;
        logger::debug() << "channel: congestion window limits may_transmit to " << allowance;
        return allowance;
    }

    logger::debug() << "channel: congestion window limits may_transmit to 0";
    pimpl_->congestion_control->cwnd_limited_ = true;
    return 0;
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
    assert(packet.size() > header_len); // Must be non-empty data packet.

    // Include implicit acknowledgment of the latest packet(s) we've acked
    uint32_t ack_seq = 
        make_second_header_word(pimpl_->state_->rx_ack_count_, pimpl_->state_->rx_ack_sequence_);
    if (pimpl_->state_->rx_unacked_)
    {
        pimpl_->state_->rx_unacked_ = 0;
        pimpl_->ack_timer_.stop();
    }

    // Send the packet
    bool success = transmit(packet, ack_seq, packet_seq, true);

    // If the retransmission timer is inactive, start it afresh.
    // (If this was a retransmission, retransmit_timeout() would have restarted it).
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
    packet_seq = pimpl_->state_->tx_sequence_;
    assert(packet_seq < max_packet_sequence);
    uint32_t tx_seq = make_first_header_word(remote_channel(), packet_seq);

    // Fill in the transmit and ACK sequence number fields.
    assert(packet.size() >= header_len);
    big_uint32_t* pkt_header = reinterpret_cast<big_uint32_t*>(packet.data());
    pkt_header[0] = tx_seq;
    pkt_header[1] = ack_seq;

    logger::file_dump dump(packet, "sending channel packet before encrypt");

    // Encrypt and compute the MAC for the packet
    byte_array epkt = armor_->transmit_encode(pimpl_->state_->tx_sequence_, packet);

    // Bump transmit sequence number,
    // and timestamp if this packet is marked for RTT measurement
    // This is the "Point of no return" -
    // a failure after this still consumes sequence number space.
    if (pimpl_->state_->tx_sequence_ == pimpl_->state_->mark_sequence_)
    {
        pimpl_->state_->mark_time_ = pimpl_->host_->current_time();
        pimpl_->state_->mark_acks_ = 0;
        pimpl_->state_->mark_base_ = pimpl_->state_->tx_ack_sequence_;
        pimpl_->state_->mark_sent_ = pimpl_->state_->tx_sequence_ - pimpl_->state_->tx_ack_sequence_;
    }
    pimpl_->state_->tx_sequence_ += 1;

    // Record the transmission event
    transmit_event_t evt(packet.size(), is_data);
    if (is_data)
    {
        pimpl_->state_->tx_inflight_count_++;
        pimpl_->state_->tx_inflight_size_ += evt.size_;
    }
    pimpl_->state_->tx_events_.push_back(evt);
    assert(pimpl_->state_->tx_event_sequence_ + pimpl_->state_->tx_events_.size()
        == pimpl_->state_->tx_sequence_);
    assert(pimpl_->state_->tx_inflight_count_ <= (unsigned)pimpl_->state_->tx_events_.size());

    logger::debug() << "Channel transmit tx seq " << pimpl_->state_->tx_sequence_
        << " size " << epkt.size();

    // Ship it out
    return send(epkt);
}

void channel::start_retransmit_timer()
{
    async::timer::duration_type timeout =
        bp::milliseconds(pimpl_->congestion_control->cumulative_rtt_.total_milliseconds() * 2);
    pimpl_->retransmit_timer_.start(timeout); // Wait for full round-trip time.
}

// channel::retransmit_timer_ invokes this slot when the retransmission timer expires.
void channel::retransmit_timeout(bool failed)
{
    logger::debug() << "Retransmit timeout" << (failed ? " - TX FAILED" : "")
        << ", interval " << pimpl_->retransmit_timer_.interval();

    // Restart the retransmission timer
    // with an exponentially increased backoff delay.
    pimpl_->retransmit_timer_.restart();

    if (!pimpl_->nocc_) {
        pimpl_->congestion_control->timeout();
    }

    // Assume that all in-flight data packets have been dropped,
    // and notify the upper layer as such.
    // Snapshot txseq first, because the missed() calls in the loop
    // might cause more packets to be transmitted.
    packet_seq_t seqlim = pimpl_->state_->tx_sequence_;
    for (packet_seq_t seq = pimpl_->state_->tx_event_sequence_; seq < seqlim; ++seq)
    {
        transmit_event_t& e = pimpl_->state_->tx_events_[seq - pimpl_->state_->tx_event_sequence_];
        if (e.pipe_)
        {
            e.pipe_ = false;
            pimpl_->state_->tx_inflight_count_--;
            pimpl_->state_->tx_inflight_size_ -= e.size_;
            missed(seq, 1);
            logger::debug() << "Retransmit timeout missed seq " << seq
                << ", in flight " << pimpl_->state_->tx_inflight_count_;
        }
    }
    if (seqlim == pimpl_->state_->tx_sequence_)
    {
        assert(pimpl_->state_->tx_inflight_count_ == 0);
        assert(pimpl_->state_->tx_inflight_size_ == 0);
    }

    // Force at least one new packet transmission regardless of cwnd.
    // This might not actually send a packet
    // if there's nothing on our transmit queue -
    // i.e., if no reliable sessions have outstanding data.
    // In that case, rtxtimer stays disarmed until the next transmit.
    on_ready_transmit();

    // If we exceed a threshold timeout, signal a failed connection.
    // The subclass has no obligation to do anything about this, however.
    set_link_status(failed ? link::status::down : link::status::stalled);
}

void channel::acknowledge(uint16_t pktseq, bool send_ack)
{
    constexpr int min_ack_packets = 2;
    constexpr int max_ack_packets = 4;

    logger::debug() << "channel: acknowledge " << pktseq
        << (send_ack ? " (sending)" : " (not sending)");

    // Update our receive state to account for this packet
    int32_t seq_diff = pktseq - pimpl_->state_->rx_ack_sequence_;
    if (seq_diff == 1)
    {
        // Received packet is in-order and contiguous.
        // Roll rx_ack_sequence_ forward appropriately.
        pimpl_->state_->rx_ack_sequence_ = pktseq;
        pimpl_->state_->rx_ack_count_++;
        pimpl_->state_->rx_ack_count_ = min(pimpl_->state_->rx_ack_count_, max_ack_count);

        // ACK the received packet if appropriate.
        // Delay our ACK for up to min_ack_packets received non-ACK-only packets,
        // or up to max_ack_packets continuous ack-only packets.
        pimpl_->state_->rx_unacked_ += 1;
        if (!send_ack and pimpl_->state_->rx_unacked_ < max_ack_packets) {
            // Only ack acks occasionally,
            // and don't start the ack timer for them.
            return;
        }
        if (pimpl_->state_->rx_unacked_ < max_ack_packets) {
            // Schedule an ack for transmission by starting the ack timer.
            // We normally do this even in for non-delayed acks,
            // so that we can process any other already-received packets first
            // and have a chance to combine multiple acks into one.
            if (pimpl_->state_->rx_unacked_ < min_ack_packets) {
                // Data packet - start delayed ack timer.
                if (!pimpl_->ack_timer_.is_active()) {
                    pimpl_->ack_timer_.start(bp::milliseconds(10));
                }
            } else {
                // Start with zero timeout - immediate callback from event loop.
                pimpl_->ack_timer_.start(bp::milliseconds(0));
            }
        } else {
            // But make sure we send an ack every max_ack_packets (4) no matter what...
            flush_ack();
        }
    }
    else if (seq_diff > 1)
    {
        // Received packet is in-order but discontiguous.
        // One or more packets probably were lost.
        // Flush any delayed ACK immediately, before updating our receive state.
        flush_ack();

        // Roll rx_ack_sequence_ forward appropriately.
        pimpl_->state_->rx_ack_sequence_ = pktseq;

        // Reset the contiguous packet counter
        pimpl_->state_->rx_ack_count_ = 0;    // (0 means 1 packet received)

        // ACK this discontiguous packet immediately
        // so that the sender is informed of lost packets ASAP.
        if (send_ack) {
            tx_ack(pimpl_->state_->rx_ack_sequence_, 0);
        }
    }
    else if (seq_diff < 0)
    {
        // Old packet recieved out of order.
        // Flush any delayed ACK immediately.
        flush_ack();

        // ACK this out-of-order packet immediately.
        if (send_ack) {
            tx_ack(pktseq, 0);
        }
    }
}

inline bool channel::tx_ack(packet_seq_t ackseq, int ack_count)
{
    byte_array pkt;
    return transmit_ack(pkt, ackseq, ack_count);
}

inline void channel::flush_ack()
{
    if (pimpl_->state_->rx_unacked_)
    {
        pimpl_->state_->rx_unacked_ = 0;
        tx_ack(pimpl_->state_->rx_ack_sequence_, pimpl_->state_->rx_ack_count_);
    }
    pimpl_->ack_timer_.stop();
}

inline void channel::ack_timeout()
{
    flush_ack();
}

bool channel::transmit_ack(byte_array& packet, packet_seq_t ackseq, int ack_count)
{
    logger::debug() << "channel: transmit_ack seq " << ackseq << ", count " << ack_count+1;

    assert(ack_count <= max_ack_count);

    if (packet.size() < header_len)
        packet.resize(header_len);

    uint32_t ack_word = make_second_header_word(ack_count, ackseq);
    packet_seq_t pktseq;

    return transmit(packet, ack_word, pktseq, false);
}

void channel::acknowledged(uint64_t txseq, int npackets, uint64_t rxackseq)
{
    logger::debug() << "Channel " << this << " - tx seqs "
        << dec << txseq << "-" << txseq+npackets-1 << " acknowledged";
}

void channel::missed(uint64_t txseq, int npackets)
{
    logger::debug() << "Channel " << this << " - tx seq " << txseq << " missed";
}

void channel::expire(uint64_t txseq, int npackets)
{
    logger::debug() << "Channel " << this << " - tx seq " << txseq << " expired";
}

void channel::receive(byte_array const& pkt, link_endpoint const& src)
{
    logger::debug() << "Channel " << this << " - receive from " << src;

    if (!is_active()) {
        logger::warning() << "Channel receive - inactive channel";
        return;
    }
    if (pkt.size() < header_len) {
        logger::warning() << "Channel receive - runt packet";
        return;
    }

    // Determine the full 64-bit packet sequence number
    uint32_t tx_seq = pkt.as<big_uint32_t>()[0];

    channel_number pktchan = tx_seq >> 24;
    assert(pktchan == local_channel());    // Enforced by link
    (void)pktchan;

    int32_t seqdiff = ((int32_t)(tx_seq << 8)
                    - ((int32_t)pimpl_->state_->rx_sequence_ << 8))
                    >> 8;

    packet_seq_t pktseq = pimpl_->state_->rx_sequence_ + seqdiff;
    logger::debug() << "Channel receive - rxseq " << pktseq << ", size " << pkt.size();

    // Immediately drop too-old or already-received packets
    static_assert(sizeof(pimpl_->state_->rx_mask_)*8 == maskBits, "Invalid rx_mask size");

    if (seqdiff > 0) {
        if (pktseq < pimpl_->state_->rx_sequence_) {
            logger::warning() << "Channel receive - 64-bit wraparound detected!";
            return;
        }
    } else if (seqdiff <= -maskBits) {
        logger::debug() << "Channel receive - too-old packet dropped";
        return;
    } else if (seqdiff <= 0) {
        if (pimpl_->state_->rx_mask_ & (1 << -seqdiff)) {
            logger::debug() << "Channel receive - duplicate packet dropped";
            return;
        }
    }

    byte_array msg = pkt;
    // Authenticate and decrypt the packet
    if (!armor_->receive_decode(pktseq, msg)) {
        logger::warning() << "Received packet auth failed on rx " << pktseq;
        return;
    }

    // Log decoded packet.
    logger::file_dump(msg, "decoded channel packet");

    // Record this packet as received for replay protection
    if (seqdiff > 0) {
        // Roll rxseq and rxmask forward appropriately.
        pimpl_->state_->rx_sequence_ = pktseq;
        // @fixme This if is not necessary...
        if (seqdiff < maskBits)
            pimpl_->state_->rx_mask_ = (pimpl_->state_->rx_mask_ << seqdiff) | 1;
        else
            pimpl_->state_->rx_mask_ = 1; // bit 0 = packet just received
    } else {
        // Set appropriate bit in rx_mask_
        assert(seqdiff < 0 and seqdiff > -maskBits);
        pimpl_->state_->rx_mask_ |= (1 << -seqdiff);
    }

    // Decode the rest of the channel header
    // This word is encrypted so take it from already decrypted byte array
    uint32_t ack_seq = msg.as<big_uint32_t>()[1];

    // Update our transmit state with the ack info in this packet
    unsigned ackct = (ack_seq >> 24) & 0xf;

    int32_t ack_diff = ((int32_t)(ack_seq << 8)
                    - ((int32_t)pimpl_->state_->tx_ack_sequence_ << 8))
                    >> 8;
    packet_seq_t ackseq = pimpl_->state_->tx_ack_sequence_ + ack_diff;
    logger::debug() << "Channel receive - ack seq " << ackseq;

    if (ackseq >= pimpl_->state_->tx_sequence_)
    {
        logger::warning() << "Channel receive - got ACK for packet seq " << ackseq
            << " not transmitted yet";
        return;
    }

    // Account for newly acknowledged packets
    unsigned new_packets = 0;

    if (ack_diff > 0)
    {
        // Received acknowledgment for one or more new packets.
        // Roll forward tx_ack_sequence_ and tx_ack_mask_.
        pimpl_->state_->tx_ack_sequence_ = ackseq;
        if (ack_diff < maskBits)
            pimpl_->state_->tx_ack_mask_ <<= ack_diff;
        else
            pimpl_->state_->tx_ack_mask_ = 0;

        // Determine the number of newly-acknowledged packets
        // since the highest previously acknowledged sequence number.
        // (Out-of-order ACKs are handled separately below.)
        new_packets = min(unsigned(ack_diff), ackct+1);

        logger::debug() << "Advanced by " << ack_diff
            << ", ack count " << ackct
            << ", new packets " << new_packets
            << ", tx ack seq " << pimpl_->state_->tx_ack_sequence_;

        // Record the new in-sequence packets in tx_ack_mask_ as received.
        // (But note: ackct+1 may also include out-of-sequence pkts.)
        pimpl_->state_->tx_ack_mask_ |= (1 << new_packets) - 1;

        // Notify the upper layer of newly-acknowledged data packets
        for (packet_seq_t seq = pimpl_->state_->tx_ack_sequence_ - new_packets + 1;
                seq <= pimpl_->state_->tx_ack_sequence_;
                ++seq)
        {
            transmit_event_t& e = pimpl_->state_->tx_events_[seq - pimpl_->state_->tx_event_sequence_];
            if (e.pipe_)
            {
                e.pipe_ = false;
                pimpl_->state_->tx_inflight_count_--;
                pimpl_->state_->tx_inflight_size_ -= e.size_;

                acknowledged(seq, 1, pktseq);
            }
        }

        // Infer that packets left un-acknowledged sufficiently late
        // have been dropped, and notify the upper layer as such.
        // XX we could avoid some of this arithmetic if we just
        // made sequence numbers start a bit higher.
        packet_seq_t miss_lim = pimpl_->state_->tx_ack_sequence_ -
            min(pimpl_->state_->tx_ack_sequence_,
                packet_seq_t(max(pimpl_->state_->miss_threshold_, new_packets)));

        for (packet_seq_t miss_seq = pimpl_->state_->tx_ack_sequence_ -
            min(pimpl_->state_->tx_ack_sequence_, 
                packet_seq_t(pimpl_->state_->miss_threshold_ + ack_diff - 1));
            miss_seq <= miss_lim;
            ++miss_seq)
        {
            transmit_event_t& e = pimpl_->state_->tx_events_[miss_seq - pimpl_->state_->tx_event_sequence_];
            if (e.pipe_)
            {
                logger::debug() << "Sequence " << pimpl_->state_->tx_event_sequence_
                    << " inferred dropped";

                e.pipe_ = false;
                pimpl_->state_->tx_inflight_count_--;
                pimpl_->state_->tx_inflight_size_ -= e.size_;

                if (!pimpl_->nocc_) {
                    pimpl_->congestion_control->missed(miss_seq);
                }

                missed(miss_seq, 1);
                logger::debug() << "Infer-missed seq " << miss_seq
                    << " tx inflight " << pimpl_->state_->tx_inflight_count_;
            }
        }

        // Finally, notice packets as they exit our ack window,
        // and garbage collect their transmit records,
        // since they can never be acknowledged after that.
        if (pimpl_->state_->tx_ack_sequence_ > maskBits)
        {
            while (pimpl_->state_->tx_event_sequence_ <= pimpl_->state_->tx_ack_sequence_ - maskBits)
            {
                logger::debug() << "Sequence " << pimpl_->state_->tx_event_sequence_ << " expired";
                assert(!pimpl_->state_->tx_events_.front().pipe_);
                pimpl_->state_->tx_events_.pop_front();
                pimpl_->state_->tx_event_sequence_++;
                expire(pimpl_->state_->tx_event_sequence_ - 1, 1);
            }
        }

        // Reset the retransmission timer, since we've made progress.
        // Only re-arm it if there's still outstanding unACKed data.
        set_link_status(link::status::up);
        if (pimpl_->state_->tx_inflight_count_ > 0)
        {
            start_retransmit_timer();
        }
        else
        {
            logger::debug() << "Stopping retransmission timer";
            pimpl_->retransmit_timer_.stop();
        }

        // Now that we've moved tx_ack_sequence_ forward to the packet's ackseq,
        // they're now the same, which is important to the code below.
        ack_diff = 0;
    }

    assert(ack_diff <= 0);

    // @todo Factor this off into a separate function (in pimpl_ perhaps).

    // Handle acknowledgments for any straggling out-of-order packets
    // (or an out-of-order acknowledgment for in-order packets).
    // Set the appropriate bits in our tx_ack_mask_,
    // and count newly acknowledged packets within our window.
    uint32_t newmask = (1 << ackct) - 1;
    if ((pimpl_->state_->tx_ack_mask_ & newmask) != newmask) {
        for (unsigned i = 0; i <= ackct; i++) {
            int bit = -ack_diff + i;
            if (bit >= maskBits)
                break;
            if (pimpl_->state_->tx_ack_mask_ & (1 << bit))
                continue;   // already ACKed
            pimpl_->state_->tx_ack_mask_ |= (1 << bit);

            transmit_event_t& e
                = pimpl_->state_->tx_events_[pimpl_->state_->tx_ack_sequence_
                    - bit - pimpl_->state_->tx_event_sequence_];

            if (e.pipe_)
            {
                e.pipe_ = false;
                pimpl_->state_->tx_inflight_count_--;
                pimpl_->state_->tx_inflight_size_ -= e.size_;

                acknowledged(pimpl_->state_->tx_ack_sequence_ - bit, 1, pktseq);
            }

            new_packets++;
        }
    }

    // Count the total number of acknowledged packets since the last mark.
    pimpl_->state_->mark_acks_ += new_packets;

    pimpl_->cc_and_rtt_update(new_packets, ackseq);

    // Pass the received packet to the upper layer for processing.
    // It'll return true if it wants us to ack the packet, false otherwise.
    if (channel_receive(pktseq, msg)) {
        acknowledge(pktseq, true);
    }
    // XX should still replay-protect even if no ack!

    // Signal upper layer that we can transmit more, if appropriate
    if (new_packets > 0 and may_transmit()) {
        on_ready_transmit();
    }
}

} // ssu namespace
