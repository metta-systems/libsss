//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "link.h"
#include "timer.h"

namespace ssu {
namespace simulation {

class simulator;
class sim_host;

// SimLink
class sim_connection
{
public:
    struct params {
        int rate;  ///< Bandwidth in bytes per second.
        async::timer::duration_type delay; ///< Connection delay.
        async::timer::duration_type queue; ///< Router queue delay.
        float loss; ///< Loss rate from 0.0 (100% reliable) to 1.0 (not delivering anything).

        std::string to_string() const;
    };

    enum preset {
        dsl_15,      ///< 1.5Mbps/384Kbps DSL link
        cable_5,     ///< 5Mbps cable modem link
        sat_10,      ///< 10Mbps satellite link with 500ms delay
        eth_10,      ///< 10Mbps Ethernet link
        eth_100,     ///< 100Mbps Ethernet link
        eth_1000,    ///< 1000Mbps Ethernet link
    };

    sim_connection(preset p = eth_100);
    ~sim_connection();

    void connect(std::shared_ptr<sim_host> downlink, endpoint downlink_address,
                 std::shared_ptr<sim_host> uplink, endpoint uplink_address);
    void disconnect();

    void set_preset(preset p);
    void set_link_params(params const& downlink, params const& uplink) {
        downlink_params_ = downlink;
        uplink_params_ = uplink;
    }
    inline void set_link_params(params const& updownlink) {
        set_link_params(updownlink, updownlink);
    }

    std::shared_ptr<sim_host> find_uplink(std::shared_ptr<sim_host> downlink) const;

private:
    std::shared_ptr<simulator> simulator_;
    std::shared_ptr<sim_host> uplink_, downlink_;
    endpoint uplink_address_, downlink_address_;
    params uplink_params_, downlink_params_;
    // Current arrival times for packets in uplink and downlink directions.
    boost::posix_time::ptime uplink_arrival_time_, downlink_arrival_time_;
};

} // simulation namespace
} // ssu namespace
