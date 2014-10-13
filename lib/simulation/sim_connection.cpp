//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include <boost/format.hpp>
#include "sss/simulation/sim_connection.h"
#include "sss/simulation/sim_host.h"
#include "arsenal/logging.h"

namespace sss {
namespace simulation {

//=================================================================================================
// This set of connection data comes straight from SST.
//=================================================================================================

#if 0
// Typical ADSL uplink/downlink bandwidths and combinations (all in Kbps)
// (e.g., see Dischinger et al, "Characterizing Residental Broadband Networks")
const int dsl_dn_bw[] = {128,256,384,512,768,1024,1536,2048,3072,4096,6144};
const int dsl_up_bw[] = {128,384,512,768};
const int dsl_bw[][2] = // specific down/up combinations
    {{128,128},{384,128},{768,384},{1024,384},{1536,384},{2048,384},
     {3072,512},{4096,512},{6144,768}};

// Typical SDSL/SHDSL bandwidth parameters (Kbps)
const int sdsl_bw[] = {512,1024,1536,2048,4096};

// Typical cable modem uplink/downlink bandwidths (all in Kbps)
const int cable_down_bw[] = {1500, 3000, 5000, 6000, 8000, 9000};
const int cable_up_bw[] = {250, 400, 500, 1000, 1500};

// Common downlink queue sizes in Kbytes, according to QFind results
// (Claypool et al, "Inferring Queue Sizes in Access Networks...")
const int dsl_dn_qsize[] = {10, 15, 25, 40, 55, 60};
const int cable_dn_qsize[] = {5, 10, 15, 20};

// Common measured last-hop link delays from Dischinger study:
// Note: round-trip, in milliseconds
const int dsl_delay[] = {7, 10, 13, 15, 20};
const int cable_delay[] = {5, 7, 10, 20};

// Common downlink and uplink queue lengths in milliseconds,
// according to Dischinger et al study
const int dsl_dn_qlen[] = {30,90,130,200,250,300,350,400};
const int dsl_up_qlen[] = {50,250,750,1200,1700,2500};
const int cable_dn_qlen[] = {30,75,130,200,250};
const int cable_up_qlen[] = {100,800,1800,2200,2500,3000,4000};

#endif

#define DSL_DN_BW   1536    // Most common ADSL link speed in 2007
#define DSL_UP_BW   384

#define CABLE_DN_BW 5000    // Most common cable link speed
#define CABLE_UP_BW 384

#define DSL_RTDELAY 13  // approx median, milliseconds
#define CABLE_RTDELAY   7   // approx median, milliseconds

#define DSL_DN_QLEN 300
#define DSL_UP_QLEN 750 // Very common among many ISPs
#define CABLE_DN_QLEN   130 // Very common among many ISPs
#define CABLE_UP_QLEN   2200

// Convert integer delay in microseconds to boost time interval
#define txdelay(value)      boost::posix_time::microseconds(value)

// Calculate transmission time of one packet in microseconds,
// given a packet size in bytes and transmission rate in bytes per second
#define txtime(bytes,rate)  boost::posix_time::microseconds((int64_t)(bytes) * 1000000 / (rate))

// Typical residential broadband DSL link
const sim_connection::params dsl15_dn =
    { DSL_DN_BW*1024/8, txdelay(DSL_RTDELAY*1000/2), txdelay(DSL_DN_QLEN*1000), 0.0 };
const sim_connection::params dsl15_up =
    { DSL_UP_BW*1024/8, txdelay(DSL_RTDELAY*1000/2), txdelay(DSL_UP_QLEN*1000), 0.0 };

// Typical residential cable modem link
const sim_connection::params cable5_dn =
    { CABLE_DN_BW*1024/8, txdelay(CABLE_RTDELAY*1000/2), txdelay(CABLE_DN_QLEN*1000), 0.0 };
const sim_connection::params cable5_up =
    { CABLE_UP_BW*1024/8, txdelay(CABLE_RTDELAY*1000/2), txdelay(CABLE_UP_QLEN*1000), 0.0 };

#define ETH10_RATE  (10*1024*1024/8)
#define ETH100_RATE (100*1024*1024/8)
#define ETH1000_RATE    (1000*1024*1024/8)

#define ETH10_DELAY (2000/2)    // XXX wild guess
#define ETH100_DELAY    (1000/2)    // XXX one data-point, YMMV
#define ETH1000_DELAY   (650/2)     // XXX one data-point, YMMV

#define ETH_MTU     1500    // Standard Ethernet MTU
#define ETH_QPKTS   25  // Typical queue length in packets (???)
#define ETH_QBYTES  (ETH_MTU * ETH_QPKTS)

// Ethernet link parameters (XXX are queue length realistic?)
const sim_connection::params eth10 =
    { ETH10_RATE, txdelay(ETH10_DELAY/2), txtime(ETH_QBYTES,ETH10_RATE), 0.0 };
const sim_connection::params eth100 =
    { ETH100_RATE, txdelay(ETH100_DELAY/2), txtime(ETH_QBYTES,ETH100_RATE), 0.0 };
const sim_connection::params eth1000 =
    { ETH1000_RATE, txdelay(ETH1000_DELAY/2), txtime(ETH_QBYTES,ETH1000_RATE), 0.0 };

// Satellite link parameters (XXX need to check)
const sim_connection::params sat10 =
    { ETH10_RATE, txdelay(500000), txdelay(1024*1024), 0.0 };

// All the values below are a wild guess!

#define WIFI_MTU     1500    // Standard Ethernet MTU
#define WIFI_QPKTS   10      // Typical queue length in packets (???)
#define WIFI_QBYTES  (WIFI_MTU * WIFI_QPKTS)

#define WIFI54_RATE  (22*1024*1024/8)
#define WIFI600_RATE (300*1024*1024/8)

const sim_connection::params wifi54 =
    { WIFI54_RATE, txdelay(500/2), txtime(WIFI_QBYTES,WIFI54_RATE), 0.05 };

const sim_connection::params wifi600 =
    { WIFI600_RATE, txdelay(100/2), txtime(WIFI_QBYTES,WIFI600_RATE), 0.07 };

//=================================================================================================
// sim_connection::params
//=================================================================================================

std::string
sim_connection::params::to_string() const
{
    std::string speed = boost::str(rate < 1024*1024
                                    ? boost::format("%fKbps") % (float(rate*8)/1024)
                                    : boost::format("%fMbps") % (float(rate*8)/(1024*1024)));
    return boost::str(boost::format("%s, delay %sms, qlen %sms (%.3f loss)")
        % speed
        % boost::posix_time::to_simple_string(delay)
        % boost::posix_time::to_simple_string(queue)
        % loss);
}

//=================================================================================================
// sim_connection
//=================================================================================================

sim_connection::sim_connection(preset p)
    : uplink_(nullptr)
    , downlink_(nullptr)
    , uplink_arrival_time_(boost::date_time::not_a_date_time)
    , downlink_arrival_time_(boost::date_time::not_a_date_time)
{
    set_preset(p);
}

sim_connection::~sim_connection()
{
    disconnect();
}

void sim_connection::connect(std::shared_ptr<sim_host> downlink, uia::comm::endpoint downlink_address,
                             std::shared_ptr<sim_host> uplink, uia::comm::endpoint uplink_address)
{
    assert(downlink != uplink);
    assert(downlink_address != uplink_address);
    assert(downlink_ == nullptr);
    assert(uplink_ == nullptr);

    downlink_ = downlink;
    uplink_ = uplink;
    downlink_address_ = downlink_address;
    uplink_address_ = uplink_address;

    downlink_->register_connection_at(downlink_address_, shared_from_this());
    uplink_->register_connection_at(uplink_address_, shared_from_this());

    downlink_arrival_time_ = downlink_->current_time();
    uplink_arrival_time_ = uplink_->current_time();
}

void sim_connection::disconnect()
{
    if (downlink_) {
        downlink_->unregister_connection_at(downlink_address_, shared_from_this());
    }
    if (uplink_) {
        uplink_->unregister_connection_at(uplink_address_, shared_from_this());
    }
}

void sim_connection::set_preset(preset p)
{
    switch (p)
    {
        case dsl_15:
            return set_link_params(dsl15_dn, dsl15_up);
        case cable_5:
            return set_link_params(cable5_dn, cable5_up);
        case sat_10:
            return set_link_params(sat10);
        case eth_10:
            return set_link_params(eth10);
        case eth_100:
            return set_link_params(eth100);
        case eth_1000:
            return set_link_params(eth1000);
        case wifi_54:
            return set_link_params(wifi54);
        case wifi_600:
            return set_link_params(wifi600);
    }
    logger::warning() << "Unknown connection preset " << p;
}

std::shared_ptr<sim_host>
sim_connection::uplink_for(std::shared_ptr<sim_host> downlink) const
{
    if (downlink == downlink_) return uplink_;
    if (downlink == uplink_) return downlink_;
    return nullptr;
}

uia::comm::endpoint
sim_connection::address_for(std::shared_ptr<sim_host> link) const
{
    if (link == downlink_) return downlink_address_;
    if (link == uplink_) return uplink_address_;
    return uia::comm::endpoint();
}

sim_connection::params const&
sim_connection::params_for(std::shared_ptr<sim_host> host) const
{
    if (host == downlink_) return downlink_params_;
    if (host == uplink_) return uplink_params_;
    throw std::runtime_error("Target host not on this connection!");
}

boost::posix_time::ptime&
sim_connection::arrival_time_for(std::shared_ptr<sim_host> host)
{
    if (host == downlink_) return downlink_arrival_time_;
    if (host == uplink_) return uplink_arrival_time_;
    throw std::runtime_error("Target host not on this connection!");
}

} // simulation namespace
} // sss namespace
