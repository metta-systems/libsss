#pragma once

#include <boost/asio.hpp> // @todo Include only header for boost::asio::ip::udp::endpoint
#include "arsenal/flurry.h"

namespace uia {
namespace comm {

class socket;

/**
 * Currently only UDP endpoints/sockets are supported.
 * System implementation might also have to work over
 * IP or even Ethernet endpoints - this will require
 * architectural change.
 */
using endpoint = boost::asio::ip::udp::endpoint;
// See below for the implementation of hash function for endpoint.

/**
 * Add an association from endpoint with particular socket.
 */
class socket_endpoint : public endpoint
{
    socket* socket_{nullptr}; ///< Associated socket, if any. @todo weak_ptr

public:
    socket_endpoint() {}
    socket_endpoint(socket_endpoint const& other) : endpoint(other), socket_(other.socket_) {}
    /**
     * This here should technically use shared_ptr and let socket_endpoint maintain a weak_ptr,
     * but practically, as the only factory creating socket_endpoints is the udp_socket,
     * it will outlive them and a simple pointer suffices.
     * @todo Make it nicer once the usage pattern is more clear here.
     */
    socket_endpoint(socket* l, endpoint const& other) : endpoint(other), socket_(l) {}

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
    socket* socket() const { return socket_; }
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
        // @todo Use hash_combine() here
        // this is a crappy slow hash until we can get ahold of a good c++1y hash
        return hash<std::string>()(a.address().to_string()) ^ (hash<int>()(a.port()) << 1);
    }
};

} // std namespace

namespace flurry {

// Flurry specialization for endpoint
inline flurry::oarchive& operator << (flurry::oarchive& oa, uia::comm::endpoint const& ep)
{
    if (ep.address().is_v4())
        oa << byte_array(ep.address().to_v4().to_bytes());
    else
        oa << byte_array(ep.address().to_v6().to_bytes());
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

