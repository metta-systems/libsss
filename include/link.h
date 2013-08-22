//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <map>
#include <unordered_map>
#include <memory>
#include <boost/asio.hpp>
#include <boost/signals2/signal.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/enable_shared_from_this.hpp>
#include "byte_array.h"
#include "asio_host_state.h"
#include "link_receiver.h"

namespace ssu {

class link;
class link_channel;

/**
 * Currently only UDP endpoints/sockets are supported.
 * System implementation might also have to work over
 * IP or even Ethernet endpoints - this will require
 * architectural change.
 */
typedef boost::asio::ip::udp::endpoint endpoint;
// See below for the implementation of hash function for endpoint.

/**
 * Add an association with particular link to the endpoint.
 */
class link_endpoint : public endpoint
{
    std::weak_ptr<link> link_; ///< Associated link, if any.
public:
    link_endpoint() {}
    link_endpoint(const link_endpoint& other) : endpoint(other), link_(other.link_) {}
    link_endpoint(const endpoint& other, std::shared_ptr<link> l) : endpoint(other), link_(l) {}

    // Send a message to this endpoint on this link
    bool send(const char *data, int size) const;
    inline bool send(const byte_array& msg) const {
        return send(msg.const_data(), msg.size());
    }

    // Obtain a shared pointer to this endpoint's link.
    // ??? FIXME
    std::shared_ptr<link> link() const { return link_.lock(); }
};

/**
 * This mixin class encapsulates link-related part of host state.
 */
class link_host_state : virtual public asio_host_state /* jeez, damn asio! */
{
    std::unordered_map<magic_t, link_receiver*> receivers;

    virtual std::unique_ptr<link> create_link() { return nullptr; }

public:
    /**
     * Create a receiver and bind it to control channel magic.
     */
    void bind_receiver(magic_t magic, link_receiver* receiver) {
        if (magic & 0xff000000) {
            throw "Invalid magic value for binding a receiver.";
        }
        receiver->magic(magic);
        receivers.insert(std::make_pair(magic, receiver)); // @todo: Will NOT replace existing element.
    }

    /**
     * Find and return a receiver for given control channel magic value.
     */
    virtual link_receiver* receiver(magic_t magic);
};

/**
 * Abstract base class for entity connecting two endpoints using some network.
 * Link manages connection lifetime and maintains the connection status info.
 * For connected links there may be a number of channels established using their own keying schemes.
 * Link orchestrates initiation of key exchanges and scheme setup.
 */
class link : public std::enable_shared_from_this<link>
{
    link_host_state& host_;
    std::map<std::pair<link_endpoint, channel_number>, link_channel*> channels_;
    bool active_{false};

    link_channel* channel(const endpoint& src, channel_number cn) { return 0; }

public:
    // ssu expresses current link status as one of three states:
    // - up: apparently alive, all's well as far as we know.
    // - stalled: briefly lost connectivity, but may be temporary.
    // - down: definitely appears to be down.
    enum class status {
        down,
        stalled,
        up
    };

    link(link_host_state& h) : host_(h) {}
    ~link();

    virtual bool send(endpoint const& ep, const char* data, size_t size) { return false; }
    inline bool send(endpoint const& ep, byte_array const& msg) {
        return send(ep, msg.const_data(), msg.size());
    }

    virtual std::vector<endpoint> local_endpoints() = 0;

    /**
     * Implementation subclass calls this method with received packets.
     * @param msg the packet received.
     * @param src the source from which the packet arrived.
     */
    void receive(const byte_array& msg, const link_endpoint& src);
};

/**
 * Class for UDP connection between two endpoints.
 * Multiplexes between channel-setup/key exchange traffic (which goes to key.cpp)
 * and per-channel data traffic (which goes to channel.cpp).
 */
class udp_link : public link
{
    boost::asio::ip::udp::socket udp_socket;
    boost::asio::streambuf received_buffer;
    link_endpoint received_from;

public:
    udp_link(const endpoint& ep, link_host_state& h);

    // bool bind(const endpoint& ep);

    /**
     * Send a packet on this UDP socket.
     * @param  ep   Target endpoint - intended receiver of the packet.
     * @param  data Packet data.
     * @param  size Packet size.
     * @return      If send was successful, i.e. the packet has been sent. It does not say anything
     *              about the reception of the packet on the other side, if it was ever delivered
     *              or accepted.
     */
    bool send(endpoint const& ep, char const* data, size_t size) override;

    // Need to duplicate it here for some reason, otherwise clients cannot use this overload. WHY?
    inline bool send(endpoint const& ep, byte_array const& msg) {
        return send(ep, msg.const_data(), msg.size());
    }

    /**
     * Return all known local endpoints referring to this link.
     */
    std::vector<endpoint> local_endpoints() override;

private:
    void prepare_async_receive();
    void udp_ready_read(const boost::system::error_code& error, std::size_t bytes_transferred);
};

} // ssu namespace

// Hash specialization for endpoint
namespace std {

template<> 
struct hash<ssu::endpoint> : public std::unary_function<ssu::endpoint, size_t>
{
    inline size_t operator()(ssu::endpoint const& /*a*/) const noexcept
    {
        size_t seed = 0;
        return seed;
    }
};

} // std namespace
