//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <boost/asio/ip/udp.hpp>
#include "arsenal/flurry.h"
#include "arsenal/hash_combine.h"

namespace uia {
namespace comm {

class socket;

/**
 * Currently only UDP endpoints/sockets are supported.
 * System implementation might also have to work over IP or even Ethernet endpoints
 * - this will require architectural change.
 */
using endpoint = boost::asio::ip::udp::endpoint;
// See below for the implementation of hash function for endpoint.

/**
 * Add an association from endpoint with particular socket.
 */
class socket_endpoint : public endpoint
{
    std::weak_ptr<socket> socket_; ///< Associated socket, if any.

public:
    socket_endpoint() {}
    socket_endpoint(socket_endpoint const& other)
        : endpoint(other)
        , socket_(other.socket_)
    {}
    socket_endpoint(std::weak_ptr<socket> l, endpoint const& other)
        : endpoint(other)
        , socket_(l)
    {}

    /**@{*/
    /**
     * Send a message to this endpoint on this socket.
     * @return true if send succeeded, false if there are problems with sending.
     */
    bool send(const char *data, int size) const;
    inline bool send(const byte_array& msg) const {
        return send(msg.const_data(), msg.size());
    }
    /**@}*/

    /**
     * Obtain a pointer to this endpoint's socket.
     */
    std::weak_ptr<socket> socket() const { return socket_; }
};

} // comm namespace
} // uia namespace

// Hash specialization for endpoint
namespace std {

template<>
struct hash<uia::comm::endpoint> : public std::unary_function<uia::comm::endpoint, size_t>
{
    inline size_t operator()(uia::comm::endpoint const& a) const noexcept
    {
        size_t seed = 0xdeadbeef;
        stdext::hash_combine(seed, a.address().to_string());
        stdext::hash_combine(seed, a.port());
        return seed;
    }
};

} // std namespace

namespace flurry {

// Flurry specialization for endpoint
inline flurry::oarchive& operator << (flurry::oarchive& oa, uia::comm::endpoint const& ep)
{
    if (ep.address().is_v4()) {
        oa << byte_array(ep.address().to_v4().to_bytes());
    }
    else {
        oa << byte_array(ep.address().to_v6().to_bytes());
    }
    oa << ep.port();
    return oa;
}

inline flurry::iarchive& operator >> (flurry::iarchive& ia, uia::comm::endpoint& ep)
{
    byte_array addr;
    uint16_t port;
    ia >> addr >> port;
    if (addr.size() == std::tuple_size<boost::asio::ip::address_v6::bytes_type>::value) { // v6 address
        boost::asio::ip::address_v6 v6(addr.as<boost::asio::ip::address_v6::bytes_type>()[0]);
        ep = uia::comm::endpoint(v6, port);
    }
    else { // v4 address
        boost::asio::ip::address_v4 v4(addr.as<boost::asio::ip::address_v4::bytes_type>()[0]);
        ep = uia::comm::endpoint(v4, port);
    }
    return ia;
}

inline flurry::oarchive& operator << (flurry::oarchive& oa, uia::comm::socket_endpoint const& ep)
{
    oa << (uia::comm::endpoint const&)ep;
    return oa;
}

inline flurry::iarchive& operator >> (flurry::iarchive& ia, uia::comm::socket_endpoint& ep)
{
    ia >> (uia::comm::endpoint&)ep;
    return ia;
}

} // flurry namespace

