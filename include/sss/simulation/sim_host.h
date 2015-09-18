//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <map>
#include <vector>
#include "sss/host.h"
#include "comm/socket.h"

namespace sss {
namespace simulation {

class simulator;
class sim_packet;
class sim_connection;
class sim_socket;

class sim_host : public host
{
    std::shared_ptr<simulator> simulator_;

    /// Virtual network connections of this host.
    std::unordered_map<uia::comm::endpoint, std::shared_ptr<sim_connection>> connections_;

    /// Sockets bound on this host by port.
    using port_t = uint16_t;
    std::unordered_map<port_t, std::shared_ptr<sim_socket>> sockets_;

    /**
     * Queue of packets to be delivered on this host.
     *
     * If the packet drops itself from this queue it's likely to be deleted as
     * this queue owns them usually.
     *
     * Standard queueu/priority_queue types are useless because they don't support the API
     * we need to manipulate the queue.
     */
    std::vector<std::shared_ptr<sim_packet>> packet_queue_;

public:
    using ptr = std::shared_ptr<sim_host>;
    using weak_ptr = std::weak_ptr<sim_host>;

public:
    std::shared_ptr<simulator> get_simulator() const { return simulator_; }

    /**
     * Use this function to create and initialize new sim_host.
     */
    static std::shared_ptr<sim_host> create(std::shared_ptr<simulator> sim);

    sim_host(std::shared_ptr<simulator> sim);
    ~sim_host();

    boost::posix_time::ptime current_time() override;
    std::unique_ptr<async::timer_engine> create_timer_engine_for(async::timer* t) override;

    std::shared_ptr<uia::comm::socket> create_socket() override;

    /** Enqueue packet, assume ownership of the packet. */
    void enqueue_packet(std::shared_ptr<sim_packet> packet);
    /** Dequeue the packet, dequeued packet will be deleted upon return. */
    void dequeue_packet(std::shared_ptr<sim_packet> packet);
    /**
     * Check if this packet is still on this host's receive queue. O(n) run time.
     * @param  packet Shared pointer to packet to find.
     * @return        True if packet is on this host's packet queue, false otherwise.
     */
    bool packet_on_queue(std::shared_ptr<sim_packet> packet) const;

    void register_connection_at(uia::comm::endpoint const& address,
        std::shared_ptr<sim_connection> conn);
    void unregister_connection_at(uia::comm::endpoint const& address,
        std::shared_ptr<sim_connection> conn);
    std::shared_ptr<sim_connection> connection_at(uia::comm::endpoint const& ep);

    void register_socket_at(uint16_t port, std::shared_ptr<sim_socket> socket);
    void unregister_socket_at(uint16_t port, std::shared_ptr<sim_socket> socket);
    std::shared_ptr<sim_socket> socket_for(uint16_t port);

    std::shared_ptr<sim_host> neighbor_at(uia::comm::endpoint const& ep, uia::comm::endpoint& src);

    // Helper for sim_socket local_endpoints().
    std::vector<uia::comm::endpoint> local_endpoints();
};

} // simulation namespace
} // sss namespace
