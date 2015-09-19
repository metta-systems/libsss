//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2015, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#define BOOST_OPTIONAL_NO_INPLACE_FACTORY_SUPPORT
#include <deque>
#include <boost/format.hpp>
#include "arsenal/make_unique.h"
#include "arsenal/logging.h"
#include "arsenal/fusionary.hpp"
#include "sss/channels/channel.h"
#include "sss/host.h"
#include "sss/internal/timer.h"
#include "sss/framing/packet_format.h"
#include "sss/framing/frame_format.h"
#include "sss/framing/framing.h"

using namespace std;
using namespace sodiumpp;
namespace asio  = boost::asio;
namespace time_ = boost::posix_time;

std::string
as_string(asio::const_buffer buf)
{
    return string(asio::buffer_cast<char const*>(buf), asio::buffer_size(buf));
}

namespace sss {

//=================================================================================================
// channel private_data implementation
//=================================================================================================

/// Size of rxmask, rxackmask, and txackmask fields in bits
static constexpr int mask_bits = 64;

static constexpr int max_ack_count = 0xf;

static constexpr unsigned CWND_MIN = 2;       // Min congestion window (packets/RTT)
static constexpr unsigned CWND_MAX = 1 << 20; // Max congestion window (packets/RTT)

static const async::timer::duration_type RTT_INIT = time_::milliseconds(500);
static const async::timer::duration_type RTT_MAX  = time_::seconds(30);

constexpr size_t channel::header_len;
constexpr packet_seq_t channel::max_packet_sequence;

struct transmit_event_t
{
    int32_t size_; ///< Total size of packet including header
    bool data_;    ///< Was an upper-layer data packet
    bool pipe_;    ///< Currently counted toward transmit_data_pipe

    inline transmit_event_t(int32_t size, bool is_data)
        : size_(size)
        , data_(is_data)
        , pipe_(is_data)
    {
        logger::debug() << "New transmission event for " << size_
                        << (data_ ? " data bytes" : " control bytes");
    }
};

//=================================================================================================

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
    time_::ptime mark_time_;
    /// Mask of packets transmitted and ACK'd (fictitious packet 0 already received)
    uint64_t tx_ack_mask_{1};
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
    uint64_t rx_mask_{1};

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

    static_assert(sizeof(tx_ack_mask_) * 8 == mask_bits, "Invalid TX ack mask size");

    shared_state(shared_ptr<host> const& host)
        : host_(host)
        , mark_time_(host->current_time())
    {
    }

    /// Compute the time elapsed since the mark.
    inline async::timer::duration_type elapsed_since_mark()
    {
        return host_->current_time() - mark_time_;
    }

    void bump_tx_sequence()
    {
        if (tx_sequence_ == mark_sequence_) {
            mark_time_ = host_->current_time();
            mark_acks_ = 0;
            mark_base_ = tx_ack_sequence_;
            mark_sent_ = tx_sequence_ - tx_ack_sequence_;
        }
        tx_sequence_ += 1;
    }
};

//=================================================================================================

/**
 * Channel's congestion control strategy.
 */
class congestion_control_strategy
{
public:
    shared_ptr<shared_state> const& state_;

    uint32_t cwnd_{CWND_MIN}; ///< Current congestion window
    bool cwnd_limited_{true}; ///< We were cwnd-limited this round-trip

    //-------------------------------------------
    /** @name Transmit state */
    //-------------------------------------------
    /**@{*/

    /// Sequence at which fast recovery finishes.
    packet_seq_t recovseq{1};

    // TCP congestion control
    uint32_t ssthresh; ///< Slow start threshold
    bool sstoggle;     ///< Slow start toggle flag for CC_VEGAS

    // Aggressive/Low-delay congestion control
    uint32_t ssbase; ///< Slow start baseline

    // Low-delay congestion control
    int cwndinc;                         // cc_delay only
    async::timer::duration_type lastrtt; ///< Measured RTT of last round-trip (cc_delay/cc_aggro)
    float lastpps;                       ///< Measured PPS of last round-trip
    uint32_t basewnd;
    float basertt, basepps, basepwr;

    /**@}*/
    //-------------------------------------------
    /** @name Channel CC statistics */
    //-------------------------------------------
    /**@{*/

    /// Cumulative measured RTT in milliseconds.
    async::timer::duration_type cumulative_rtt_;
    float cumulative_rtt_variance_; ///< Cumulative variation in RTT
    float cumulative_pps_;          ///< Cumulative measured packets per second
    float cumulative_pps_var;       ///< Cumulative variation in PPS
    float cumpwr;                   ///< Cumulative measured network power (pps/rtt)
    float cumbps;                   ///< Cumulative measured bytes per second
    float cumloss;                  ///< Cumulative measured packet loss ratio

    /**@}*/

    congestion_control_strategy(shared_ptr<shared_state> const& state)
        : state_(state)
    {
        reset();
    }

    /// Reset congestion control.
    void reset();
    /// Update congestion control on missed packet ack.
    virtual void missed(uint64_t pktseq);
    /// Update on expired packet.
    virtual void timeout();
    /// Update on newly received acks.
    virtual void update(unsigned new_packets) = 0;
    /// Update rtt information.
    virtual void rtt_update(float pps, float rtt) = 0;
    /// Print cumulative rtt statistics to the log
    void log_rtt_stats();
    /// Update rtt cumulative statistics.
    void stats_update(float& pps_out, float& rtt_out);

    /// for cc_fixed: fixed congestion window for reserved-bandwidth links
    inline void set_cc_window(uint32_t wnd) { cwnd_ = wnd; }

    /// Congestion information accessors for flow monitoring purposes
    inline uint32_t tx_congestion_window() const { return cwnd_; }
    inline uint32_t tx_bytes_in_flight() const { return state_->tx_inflight_size_; }
    inline uint32_t tx_packets_in_flight() const { return state_->tx_inflight_count_; }
};

void
congestion_control_strategy::reset()
{
    logger::debug() << "CC reset";
    cwnd_                    = CWND_MIN;
    cwnd_limited_            = true;
    ssthresh                 = CWND_MAX;
    sstoggle                 = true;
    ssbase                   = 0;
    cwndinc                  = 1;
    lastrtt                  = time_::milliseconds(0);
    lastpps                  = 0;
    basertt                  = 0;
    basepps                  = 0;
    cumulative_rtt_          = RTT_INIT;
    cumulative_rtt_variance_ = 0;
    cumulative_pps_          = 0;
    cumulative_pps_var       = 0;
    cumpwr                   = 0;
    cumbps                   = 0;
    cumloss                  = 0;
}

void
congestion_control_strategy::missed(uint64_t pktseq)
{
    logger::debug() << "Missed seq " << pktseq;

    // This basic missed packet calculation is shared by cc_tcp, cc_delay and cc_vegas:

    // Packet loss detected -
    // perform standard TCP congestion control
    if (pktseq <= recovseq) {
        // We're in a fast recovery window:
        // this isn't a new loss event.
        return;
    }

    // new loss event: cut ssthresh and cwnd
    // ssthresh = (tx_sequence_ - tx_ack_sequence_) / 2;    XXX
    ssthresh = cwnd_ / 2;
    ssthresh = max(ssthresh, CWND_MIN);
    // logger::debug() << "%d PACKETS LOST: cwnd %d -> %d", ackdiff - newpackets, cwnd, ssthresh);
    cwnd_ = ssthresh;

    // fast recovery for the rest of this window
    recovseq = state_->tx_sequence_;
}

void
congestion_control_strategy::timeout()
{
    // If fixed cwnd, no congestion control, otherwise
    // Reset cwnd and go back to slow start
    ssthresh = state_->tx_inflight_count_ / 2;
    ssthresh = max(ssthresh, CWND_MIN);
    cwnd_ = CWND_MIN;
    logger::debug() << "CC retransmit timeout: ssthresh=" << ssthresh << ", cwnd=" << cwnd_;
}

void
congestion_control_strategy::log_rtt_stats()
{
    logger::debug() << boost::format(
                           "Cumulative: rtt %.3f[±%.3f] pps %.3f[±%.3f] pwr %.3f loss %.3f")
                           % cumulative_rtt_ % cumulative_rtt_variance_ % cumulative_pps_
                           % cumulative_pps_var % cumpwr % cumloss;
}

void
congestion_control_strategy::stats_update(float& pps_out, float& rtt_out)
{
    // 'rtt' is the total round-trip delay in microseconds before
    // we receive an ACK for a packet at or beyond the mark.
    // Fold this into 'rtt' to determine avg round-trip time,
    // and restart the timer to measure the next round-trip.
    async::timer::duration_type rtt = state_->elapsed_since_mark();
    rtt                             = max(time_::time_duration(time_::microseconds(1)), min(RTT_MAX, rtt));
    cumulative_rtt_ = time_::microseconds(
        (cumulative_rtt_.total_microseconds() * 7.0 + rtt.total_microseconds()) / 8.0);

    rtt_out = rtt.total_microseconds();

    // Compute an RTT variance measure
    float rttvar             = fabs((rtt - cumulative_rtt_).total_microseconds());
    cumulative_rtt_variance_ = ((cumulative_rtt_variance_ * 7.0) + rttvar) / 8.0;

    // 'mark_acks_' is the number of unique packets ACKed
    // by the receiver during the time since the last mark.
    // Use this to gauge throughput during this round-trip.
    float pps       = (float)state_->mark_acks_ * 1000000.0 / rtt.total_microseconds();
    cumulative_pps_ = ((cumulative_pps_ * 7.0) + pps) / 8.0;

    pps_out = pps;

    // "Power" measures network efficiency
    // in the sense of both minimizing rtt and maximizing pps.
    float pwr = pps / rtt.total_microseconds();
    cumpwr    = ((cumpwr * 7.0) + pwr) / 8.0;

    // Compute a PPS variance measure
    float ppsvar       = fabsf(pps - cumulative_pps_);
    cumulative_pps_var = ((cumulative_pps_var * 7.0) + ppsvar) / 8.0;

    // Calculate loss rate during this last round-trip,
    // and a cumulative loss ratio.
    // Could go out of (0.0,1.0) range due to out-of-order acks.
    float loss = (float)(state_->mark_sent_ - state_->mark_acks_) / (float)state_->mark_sent_;
    loss       = max(0.0f, min(1.0f, loss));
    cumloss    = ((cumloss * 7.0) + loss) / 8.0;

    // Reset pimpl_->mark_sequence_ to be the next packet transmitted.
    // The new timestamp will be taken when that packet is sent.
    state_->mark_sequence_ = state_->tx_sequence_;

    lastrtt = rtt;
    lastpps = pps;
}

//=================================================================================================
// Congestion Control strategies.
//=================================================================================================

/**
 * TCP-like congestion control.
 */
class cc_tcp : public congestion_control_strategy
{
public:
    cc_tcp(shared_ptr<shared_state> const& state)
        : congestion_control_strategy(state)
    {
    }
    void rtt_update(float pps, float rtt) override;
    void update(unsigned new_packets) override;
};

void
cc_tcp::rtt_update(float pps, float rtt)
{
    // Normal TCP congestion control: during congestion avoidance,
    // increment cwnd once each RTT, but only on round-trips that were cwnd-limited.
    if (cwnd_limited_) {
        cwnd_++;
        logger::debug() << "cwnd increased to " << cwnd_ << ", ssthresh " << ssthresh;
    }
    cwnd_limited_ = false;
}

void
cc_tcp::update(unsigned new_packets)
{
    // During standard TCP slow start procedure,
    // increment cwnd for each newly-ACKed packet.
    // XX TCP spec allows this to be <=,
    // which puts us in slow start briefly after each loss...
    if (new_packets and cwnd_limited_ and cwnd_ < ssthresh) {
        cwnd_ = min(cwnd_ + new_packets, ssthresh);
        logger::debug() << "Slow start: " << new_packets << " new ACKs; boost cwnd to " << cwnd_
                        << " (ssthresh " << ssthresh << ")";
    }
}

/**
 * Aggressive congestion control.
 */
class cc_aggressive : public congestion_control_strategy
{
public:
    cc_aggressive(shared_ptr<shared_state> const& state)
        : congestion_control_strategy(state)
    {
    }
    void missed(uint64_t pktseq) override;
    void rtt_update(float pps, float rtt) override;
    void update(unsigned new_packets) override;
};

void
cc_aggressive::missed(uint64_t pktseq)
{
    // Number of packets we think have been lost
    // so far during this round-trip.
    int lost = (state_->tx_ack_sequence_ - state_->mark_base_) - state_->mark_acks_;
    lost     = max(0, lost);

    // Number of packets we expect to receive,
    // assuming the lost ones are really lost
    // and we don't lose any more this round-trip.
    unsigned expected = state_->mark_sent_ - lost;

    // Clamp the congestion window to this value.
    if (expected < cwnd_) {
        logger::debug() << "PACKETS LOST: cwnd " << cwnd_ << "->" << expected;
        cwnd_ = ssbase = expected;
        cwnd_ = max(CWND_MIN, cwnd_);
    }
}

void
cc_aggressive::rtt_update(float pps, float rtt)
{
    // aggressive doesn't track RTT
}

void
cc_aggressive::update(unsigned new_packets)
{
    // We're always in slow start, but we only count ACKs received
    // on schedule and after a per-roundtrip baseline.
    if (state_->mark_acks_ > ssbase and state_->elapsed_since_mark() <= lastrtt) {
        cwnd_ += min(new_packets, state_->mark_acks_ - ssbase);
        logger::debug() << "Slow start: " << new_packets << " new ACKs; boost cwnd to " << cwnd_;
    }
}

/**
 * Low-delay congestion control.
 */
class cc_delay : public congestion_control_strategy
{
public:
    cc_delay(shared_ptr<shared_state> const& state)
        : congestion_control_strategy(state)
    {
    }
    void rtt_update(float pps, float rtt) override;
    void update(unsigned new_packets) override;
};

void
cc_delay::rtt_update(float pps, float rtt)
{
    float pwr = pps / rtt;
    if (pwr > basepwr) {
        basepwr = pwr;
        basertt = rtt;
        basepps = pps;
        basewnd = state_->mark_acks_;
    } else if (state_->mark_acks_ <= basewnd and rtt > basertt) {
        basertt = rtt;
        basepwr = basepps / basertt;
    } else if (state_->mark_acks_ >= basewnd and pps < basepps) {
        basepps = pps;
        basepwr = basepps / basertt;
    }

    if (cwndinc > 0) {
        // Window going up.
        // If RTT makes a significant jump, reverse.
        if (rtt > basertt or cwnd_ >= CWND_MAX) {
            cwndinc = -1;
        } else {
            // Additively increase the window
            cwnd_ += cwndinc;
        }
    } else {
        // Window going down.
        // If PPS makes a significant dive, reverse.
        if (pps < basepps or cwnd_ <= CWND_MIN) {
            ssbase  = cwnd_++;
            cwndinc = +1;
        } else {
            // Additively decrease the window
            cwnd_ += cwndinc;
        }
    }
    cwnd_ = max(CWND_MIN, cwnd_);
    cwnd_ = min(CWND_MAX, cwnd_);

    logger::debug() << boost::format(
                           "RT: pwr %.0f[%.0f/%.0f]@%d base %.0f[%.0f/%.0f]@%d cwnd %d%+d")
                           % (pwr * 1000.0) % pps % rtt % state_->mark_acks_ % (basepwr * 1000.0)
                           % basepps % basertt % basewnd % cwnd_ % cwndinc;
}

void
cc_delay::update(unsigned new_packets)
{
    if (cwndinc < 0) { // Only slow start during up-phase
        return;
    }

    // call into cc_aggressive::update() here?
    // hrm, what about non-overridden methods? well, missed() is different, might need override too
    // for now just copypasted the duplicated code...

    // We're always in slow start, but we only count ACKs received
    // on schedule and after a per-roundtrip baseline.
    if (state_->mark_acks_ > ssbase and state_->elapsed_since_mark() <= lastrtt) {
        cwnd_ += min(new_packets, state_->mark_acks_ - ssbase);
        logger::debug() << "Slow start: " << new_packets << " new ACKs; boost cwnd to " << cwnd_;
    }
}

/**
 * TCP/Vegas congestion control.
 */
class cc_vegas : public congestion_control_strategy
{
public:
    cc_vegas(shared_ptr<shared_state> const& state)
        : congestion_control_strategy(state)
    {
    }
    void rtt_update(float pps, float rtt) override;
    void update(unsigned new_packets) override;
};

void
cc_vegas::rtt_update(float pps, float rtt)
{
    // Keep track of the lowest RTT ever seen,
    // as per the original Vegas algorithm.
    // This has the known problem that it screws up
    // if the path's actual base RTT changes.
    if (basertt == 0) // first packet
        basertt = rtt;
    else if (rtt < basertt)
        basertt = rtt;
    // else
    //  basertt = (basertt * 255.0 + rtt) / 256.0;

    float expect  = (float)state_->mark_sent_ / basertt;
    float actual  = (float)state_->mark_sent_ / rtt;
    float diffpps = expect - actual;
    assert(diffpps >= 0.0);
    float diffpprt = diffpps * rtt;

    if (diffpprt < 1.0 and cwnd_ < CWND_MAX and cwnd_limited_) {
        cwnd_++;
        // ssthresh = max(ssthresh, cwnd / 2); ??
    } else if (diffpprt > 3.0 and cwnd_ > CWND_MIN) {
        cwnd_--;
        ssthresh = min(ssthresh, cwnd_); // /2??
    }

    logger::debug() << boost::format(
                           "Round-trip: win %d basertt %.3f rtt %d "
                           "exp-pps %f act-pps %f diff-pprt %.3f cwnd %d")
                           % state_->mark_sent_ % basertt % rtt % (expect * 1000000.0)
                           % (actual * 1000000.0) % diffpprt % cwnd_;
}

void
cc_vegas::update(unsigned new_packets)
{
    sstoggle = !sstoggle;
    if (sstoggle)
        return; // do slow start only once every two RTTs

    // call into cc_tcp::update()
    // for now copypasted

    // During standard TCP slow start procedure,
    // increment cwnd for each newly-ACKed packet.
    // XX TCP spec allows this to be <=,
    // which puts us in slow start briefly after each loss...
    if (new_packets and cwnd_limited_ and cwnd_ < ssthresh) {
        cwnd_ = min(cwnd_ + new_packets, ssthresh);
        logger::debug() << "Slow start: " << new_packets << " new ACKs; boost cwnd to " << cwnd_
                        << " (ssthresh " << ssthresh << ")";
    }
}

/**
 * CTCP-like congestion control.
 */
class cc_ctcp : public congestion_control_strategy
{
public:
    cc_ctcp(shared_ptr<shared_state> const& state)
        : congestion_control_strategy(state)
    {
    }
    void missed(uint64_t pktseq) override;
    void rtt_update(float pps, float rtt) override;
    void update(unsigned new_packets) override;
};

void
cc_ctcp::missed(uint64_t pktseq)
{
    assert(0); // XXX
}

void
cc_ctcp::rtt_update(float pps, float rtt)
{
#if 0
    k = 0.8; a = 1/8; B = 1/2
    if (in-recovery)
        ...
    else if (diff < y) {
        dwnd += sqrt(win)/8.0 - 1;
    } else
        dwnd -= C * diff;
#endif
}

void
cc_ctcp::update(unsigned new_packets)
{
    assert(0); // XXX
}

/**
 * No congestion control, fixed window.
 */
class cc_fixed : public congestion_control_strategy
{
public:
    cc_fixed(shared_ptr<shared_state> const& state)
        : congestion_control_strategy(state)
    {
    }
    void missed(uint64_t pktseq) override;
    void timeout() override;
    void update(unsigned new_packets) override;
};

void
cc_fixed::missed(uint64_t pktseq)
{
    // fixed cwnd, no congestion control
}

void
cc_fixed::timeout()
{
    // fixed cwnd, no congestion control
}

void
cc_fixed::update(unsigned new_packets)
{
    // fixed cwnd, no congestion control
}

//=================================================================================================
// Channel's private state.
//=================================================================================================

class channel::private_data
{
public:
    shared_ptr<host> host_;
    shared_ptr<shared_state> state_;

    //-------------------------------------------
    // Congestion control
    //-------------------------------------------
    unique_ptr<congestion_control_strategy> congestion_control;
    bool nocc_{false};

    // bool delayack;      ///< Enable delayed acknowledgments
    async::timer ack_timer_; ///< Delayed ACK timer.

    // Retransmit state
    async::timer retransmit_timer_; ///< Retransmit timer.

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

    ~private_data() { logger::debug() << "~channel::private_data"; }

    void cc_and_rtt_update(unsigned new_packets, packet_seq_t ackseq);

    void stats_timeout();

    void reset_congestion_control();

    /// Compute current number of transmitted but un-acknowledged packets.
    /// This count may include raw ACK packets, for which we expect no acknowledgments
    /// unless they happen to be piggybacked on data coming back.
    inline int64_t unacked_packets() { return state_->tx_sequence_ - state_->tx_ack_sequence_; }
};

void
channel::private_data::reset_congestion_control()
{
    // Initialize congestion control state
    congestion_control.reset(new cc_tcp(state_));

    // --CC control---------------------------------------------------
    // @todo Move this to cc_strategy implementation.
    // delayack = true;
    // --end CC control-----------------------------------------------

    // Statistics gathering state
    stats_timer_.on_timeout.connect([this](bool) { stats_timeout(); });
    stats_timer_.start(time_::seconds(5));
}

// Transmit statistics
void
channel::private_data::stats_timeout()
{
    logger::info() << boost::format(
                          "STATS: txseq %llu, txackseq %llu, rxseq %llu, rxackseq %llu, "
                          "txfltcnt %d, cwnd %d, ssthresh %d, "
                          "cumrtt %.3f, cumpps %.3f, cumloss %.3f")
                          % state_->tx_sequence_ % state_->tx_ack_sequence_ % state_->rx_sequence_
                          % state_->rx_ack_sequence_ % state_->tx_inflight_count_
                          % congestion_control->cwnd_ % congestion_control->ssthresh
                          % congestion_control->cumulative_rtt_
                          % congestion_control->cumulative_pps_ % congestion_control->cumloss;
}

void
channel::private_data::cc_and_rtt_update(unsigned new_packets, packet_seq_t ackseq)
{
    if (!nocc_) {
        congestion_control->update(new_packets);
    }

    // When ackseq passes mark_sequence_, we've observed a round-trip,
    // so update our round-trip statistics.
    if (ackseq >= state_->mark_sequence_) {
        float pps, rtt;
        congestion_control->stats_update(pps, rtt);

        if (!nocc_) {
            congestion_control->rtt_update(pps, rtt);
            congestion_control->log_rtt_stats();
        } else {
            logger::debug() << "End-to-end rtt " << rtt << " cumulative rtt "
                            << congestion_control->cumulative_rtt_; // fixme, nullptr access?
        }
    }

    // Always clamp cwnd against CWND_MAX.
    congestion_control->cwnd_ = min(congestion_control->cwnd_, CWND_MAX);
}

//=================================================================================================
// channel
//=================================================================================================

channel::channel(shared_ptr<host> host, secret_key local, public_key remote)
    : socket_channel()
    , pimpl_(stdext::make_unique<private_data>(host))
    , local_key_(local)
    , remote_key_(remote)
{
    pimpl_->retransmit_timer_.on_timeout.connect([this](bool fail) { retransmit_timeout(fail); });

    // Delayed ACK state
    pimpl_->ack_timer_.on_timeout.connect([this](bool) { ack_timeout(); });
}

channel::~channel()
{
}

shared_ptr<host>
channel::get_host()
{
    return pimpl_->host_;
}

void
channel::start(bool initiate)
{
    logger::debug() << "Channel - start as " << (initiate ? "initiator" : "responder");

    super::start(initiate);

    pimpl_->nocc_ = is_congestion_controlled();

    // We're ready to go!
    set_link_status(uia::comm::socket::status::up);
    on_ready_transmit();
    start_retransmit_timer();
}

void
channel::stop()
{
    logger::debug() << "Channel - stop";
    pimpl_->retransmit_timer_.stop();
    pimpl_->ack_timer_.stop();
    pimpl_->stats_timer_.stop();

    super::stop();

    set_link_status(uia::comm::socket::status::down);
}

size_t
channel::may_transmit()
{
    logger::debug(200) << "Channel - may_transmit";
    if (pimpl_->nocc_) {
        return super::may_transmit();
    }

    if (pimpl_->congestion_control->cwnd_ > pimpl_->state_->tx_inflight_count_) {
        int allowance = pimpl_->congestion_control->cwnd_ - pimpl_->state_->tx_inflight_count_;
        logger::debug(200) << "Channel - congestion window limits may_transmit to " << allowance;
        return allowance;
    }

    logger::debug(200) << "Channel - congestion window limits may_transmit to 0";
    pimpl_->congestion_control->cwnd_limited_ = true;
    return 0;
}

bool
channel::channel_transmit(boost::asio::const_buffer packet, packet_seq_t& packet_seq)
{
    // assert(packet.size() > header_len); // Must be non-empty data packet.

    // Include implicit acknowledgment of the latest packet(s) we've acked
    uint32_t ack_seq = 0;
    // make_second_header_word(pimpl_->state_->rx_ack_count_, pimpl_->state_->rx_ack_sequence_);

    // @todo verify that ackct is always 0xf in case no packets were dropped
    // (that can nicely be a unit test)
    // override channel's armor to a fixture one, with transmit_encode() actually examining
    // packets to be sent and receive_decode() examining packets "received".
    // then prove that ackct goes up from 1 to 15 and stays there.

    if (pimpl_->state_->rx_unacked_) {
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

bool
channel::transmit(boost::asio::const_buffer packet,
                  uint32_t ack_seq,
                  uint64_t& packet_seq,
                  bool is_data)
{
    assert(is_active());

    logger::debug() << "Channel sending a packet";

    // Don't allow tx_sequence_ counter to wrap (@fixme re-key before it does!)
    // packet_seq = pimpl_->state_->tx_sequence_;
    // assert(packet_seq < max_packet_sequence);
    // uint32_t tx_seq = packet_seq;

    // Fill in the transmit and ACK sequence number fields.
    // assert(packet.size() >= header_len);
    // big_uint32_t* pkt_header = packet.as<big_uint32_t>(2);
    // pkt_header[0]            = tx_seq;
    // pkt_header[1]            = ack_seq;

    // logger::file_dump(packet, "sending channel packet before encrypt");

    // // Encrypt and compute the MAC for the packet
    // byte_array epkt = transmit_encode(asio::mutable_buffer(packet.data(), packet.size()));

    // logger::file_dump(epkt, "sending channel packet after encrypt");

    // // Bump transmit sequence number,
    // // and timestamp if this packet is marked for RTT measurement
    // // This is the "Point of no return" -
    // // a failure after this still consumes sequence number space.
    // pimpl_->state_->bump_tx_sequence();

    // // Record the transmission event
    // transmit_event_t evt(packet.size(), is_data);
    // if (is_data) {
    //     pimpl_->state_->tx_inflight_count_++;
    //     pimpl_->state_->tx_inflight_size_ += evt.size_;
    // }
    // pimpl_->state_->tx_events_.push_back(evt);
    // assert(pimpl_->state_->tx_event_sequence_ + pimpl_->state_->tx_events_.size()
    //        == pimpl_->state_->tx_sequence_);
    // assert(pimpl_->state_->tx_inflight_count_ <= (unsigned)pimpl_->state_->tx_events_.size());

    // logger::debug() << "Channel transmit tx seq " << dec << pimpl_->state_->tx_sequence_ << "
    // size "
    //                 << epkt.size();

    // Ship it out
    // return send(epkt);
    return false;
}

void
channel::start_retransmit_timer()
{
    async::timer::duration_type timeout =
        time_::milliseconds(pimpl_->congestion_control->cumulative_rtt_.total_milliseconds() * 2);
    pimpl_->retransmit_timer_.start(timeout); // Wait for full round-trip time.
}

// channel::retransmit_timer_ invokes this slot when the retransmission timer expires.
void
channel::retransmit_timeout(bool failed)
{
    logger::debug() << "Retransmit timeout" << (failed ? " - TX FAILED" : "") << ", interval "
                    << pimpl_->retransmit_timer_.interval();

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
    for (packet_seq_t seq = pimpl_->state_->tx_event_sequence_; seq < seqlim; ++seq) {
        transmit_event_t& e = pimpl_->state_->tx_events_[seq - pimpl_->state_->tx_event_sequence_];
        if (e.pipe_) {
            e.pipe_ = false;
            pimpl_->state_->tx_inflight_count_--;
            pimpl_->state_->tx_inflight_size_ -= e.size_;
            missed(seq, 1);
            logger::debug() << "Retransmit timeout missed seq " << seq << ", in flight "
                            << pimpl_->state_->tx_inflight_count_;
        }
    }
    if (seqlim == pimpl_->state_->tx_sequence_) {
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
    set_link_status(failed ? uia::comm::socket::status::down : uia::comm::socket::status::stalled);
}

void
channel::acknowledge(packet_seq_t pktseq, bool send_ack)
{
    constexpr int min_ack_packets = 2;
    constexpr int max_ack_packets = 4;

    logger::debug() << "Channel - acknowledge " << pktseq
                    << (send_ack ? " (sending)" : " (not sending)");

    // Update our receive state to account for this packet
    int32_t seq_diff = pktseq - pimpl_->state_->rx_ack_sequence_;
    if (seq_diff == 1) {
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
                    pimpl_->ack_timer_.start(time_::milliseconds(10));
                }
            } else {
                // Start with zero timeout - immediate callback from event loop.
                pimpl_->ack_timer_.start(time_::milliseconds(0));
            }
        } else {
            // But make sure we send an ack every max_ack_packets (4) no matter what...
            flush_ack();
        }
    } else if (seq_diff > 1) {
        // Received packet is in-order but discontiguous.
        // One or more packets probably were lost.
        // Flush any delayed ACK immediately, before updating our receive state.
        flush_ack();

        // Roll rx_ack_sequence_ forward appropriately.
        pimpl_->state_->rx_ack_sequence_ = pktseq;

        // Reset the contiguous packet counter
        pimpl_->state_->rx_ack_count_ = 0; // (0 means 1 packet received)

        // ACK this discontiguous packet immediately
        // so that the sender is informed of lost packets ASAP.
        if (send_ack) {
            tx_ack(pimpl_->state_->rx_ack_sequence_, 0);
        }
    } else if (seq_diff < 0) {
        // Old packet recieved out of order.
        // Flush any delayed ACK immediately.
        flush_ack();

        // ACK this out-of-order packet immediately.
        if (send_ack) {
            tx_ack(pktseq, 0);
        }
    }
}

inline bool
channel::tx_ack(packet_seq_t ackseq, int ack_count)
{
    byte_array pkt;
    return transmit_ack(pkt, ackseq, ack_count);
}

inline void
channel::flush_ack()
{
    if (pimpl_->state_->rx_unacked_) {
        pimpl_->state_->rx_unacked_ = 0;
        tx_ack(pimpl_->state_->rx_ack_sequence_, pimpl_->state_->rx_ack_count_);
    }
    pimpl_->ack_timer_.stop();
}

inline void
channel::ack_timeout()
{
    flush_ack();
}

bool
channel::transmit_ack(byte_array& packet, packet_seq_t ackseq, int ack_count)
{
    logger::debug() << "Channel - transmit_ack seq " << ackseq << ", count " << ack_count + 1;

    // assert(ack_count <= max_ack_count);

    // if (packet.size() < header_len)
    // packet.resize(header_len);

    // uint32_t ack_word = 0; // make_second_header_word(ack_count, ackseq);
    // packet_seq_t pktseq;

    // return transmit(packet, ack_word, pktseq, false);
    return false;
}

void
channel::acknowledged(uint64_t txseq, int npackets, uint64_t rxackseq)
{
    logger::debug() << "Channel " << this << " - tx seqs " << dec << txseq << "-"
                    << txseq + npackets - 1 << " acknowledged";
}

void
channel::missed(uint64_t txseq, int npackets)
{
    logger::debug() << "Channel " << this << " - tx seq " << txseq << " missed";
}

void
channel::expire(uint64_t txseq, int npackets)
{
    logger::debug() << "Channel " << this << " - tx seq " << txseq << " expired";
}

// Determine the full 64-bit packet sequence number
packet_seq_t
channel::derive_packet_seq(packet_seq_t tx_seq)
{
    // kill high 8 bits (channel number)
    int32_t seqdiff = ((int32_t)(tx_seq << 8) - ((int32_t)pimpl_->state_->rx_sequence_ << 8)) >> 8;

    packet_seq_t pktseq = pimpl_->state_->rx_sequence_ + seqdiff;

    // Immediately drop too-old or already-received packets
    static_assert(sizeof(pimpl_->state_->rx_mask_) * 8 == mask_bits, "Invalid RX mask size");

    if (seqdiff > 0) {
        if (pktseq < pimpl_->state_->rx_sequence_) {
            logger::warning() << "Channel receive - 64-bit wraparound detected!";
            return 0;
        }
    }
    return pktseq;
}

void
channel::runt_packet_received(uia::comm::socket_endpoint const& /*src*/)
{
    ++runt_packets_;
}

void
channel::bad_auth_received(uia::comm::socket_endpoint const& /*src*/)
{
    ++bad_auth_packets_;
}

bool
channel::receive_decode(asio::const_buffer in, byte_array& out)
{
    try {
        sss::channels::message_packet_header msg;
        in = fusionary::read(msg, in);

        assert(asio::buffer_size(in) == 0);

        string nonce = MESSAGE_NONCE_PREFIX + as_string(msg.nonce);
        unboxer<recv_nonce> unseal(as_string(msg.shortterm_public_key), local_key_, nonce);

        out = unseal.unbox(msg.box.data);
    } catch (char const* err) {
        logger::warning() << err;
        return false;
    }
    return true;
}

void
channel::receive(asio::const_buffer pkt, uia::comm::socket_endpoint const& src)
{
    logger::debug() << "Channel " << this << " - receive from " << src;

    if (!is_active()) {
        logger::warning() << "Channel receive - inactive channel";
        return;
    }
    if (asio::buffer_size(pkt) < MIN_PACKET_SIZE) {
        logger::warning() << "Channel receive - runt packet";
        runt_packet_received(src);
        return;
    }

    byte_array msg;

    // channel receives only MESSAGE packets, therefore receive_decode
    // is rather straightforward

    // Authenticate and decrypt the packet
    if (!receive_decode(pkt, msg)) {
        logger::warning() << "Received packet auth failed";
        bad_auth_received(src);
        return;
    }

    // Log decoded packet.
    logger::file_dump(msg, "decoded channel packet");

    sss::framing::packet_header phdr;
    asio::const_buffer packet_buf(asio::buffer(msg.as_string()));
    // if (phdr.flags bitand flags::fec) {
    // Insert packet to FEC queue
    // }
    // else
    sss::framing::framing_t fr(this->enable_shared_from_this<channel>::shared_from_this());
    fr.deframe(packet_buf);

    // packet_seq_t pktseq = derive_packet_seq(phdr.packet_sequence.value());

    // Signal upper layer that we can transmit more, if appropriate
    if (/*new_packets > 0 and*/ may_transmit()) {
        on_ready_transmit();
    }
}

} // sss namespace
