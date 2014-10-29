//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

namespace sss {
namespace decongestion {

/**
 * Channel's congestion control strategy.
 */
class decongestion_strategy
{
public:
    /// How many packets can still be sent without congesting the uplink?
    virtual size_t tx_window() = 0;

    /// Reset congestion control.
    virtual void reset();
    /// Update congestion control on missed packet.
    virtual void missed(packet_seq_t pktseq);
    /// Update on expired packet.
    virtual void timeout();
    /// Update on newly received ACKs.
    virtual void update(unsigned new_packets/*@todo*/) = 0;
    /// Update rtt information.
    virtual void rtt_update(float packets_per_sec, float round_trip_time) = 0;

    /// Print cumulative rtt statistics to the log
    void log_rtt_stats();
    /// Update rtt cumulative statistics.
    void stats_update(float& pps_out, float& rtt_out);
};

} // decongestion namespace
} // sss namespace
