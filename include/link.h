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
#include <unordered_set>
#include <memory>
#include <boost/asio.hpp>
#include <boost/signals2/signal.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/enable_shared_from_this.hpp>
#include "byte_array.h"
#include "asio_host_state.h"
#include "link_receiver.h"
#include "flurry.h"
#include "algorithm.h"

class settings_provider;

namespace ssu {

class host;
class link;
class link_channel;

/**
 * Currently only UDP endpoints/sockets are supported.
 * System implementation might also have to work over
 * IP or even Ethernet endpoints - this will require
 * architectural change.
 */
using endpoint = boost::asio::ip::udp::endpoint;
// See below for the implementation of hash function for endpoint.

/**
 * Add an association from endpoint with particular link.
 */
class link_endpoint : public endpoint
{
    link* link_{nullptr}; ///< Associated link, if any.

public:
    link_endpoint() {}
    link_endpoint(link_endpoint const& other) : endpoint(other), link_(other.link_) {}
    /**
     * This here should technically use shared_ptr and let link_endpoint maintain a weak_ptr,
     * but practically, as the only factory creating link_endpoints is the udp_link,
     * it will outlive them and a simple pointer suffices.
     * @todo Make it nicer once the usage pattern is more clear here.
     */
    link_endpoint(link* l, endpoint const& other) : endpoint(other), link_(l) {}

    /**@{*/
    /**
     * Send a message to this endpoint on this link.
     * @return true if send succeeded, false if there are problems with sending.
     */
    bool send(const char *data, int size) const;
    inline bool send(const byte_array& msg) const {
        return send(msg.const_data(), msg.size());
    }
    /**@}*/

    /**
     * Obtain a pointer to this endpoint's link.
     */
    link* link() const { return link_; }
};

/**
 * This mixin class encapsulates link-related part of host state.
 * @see host
 */
class link_host_state : virtual public asio_host_state /* jeez, damn asio! */
{
    /**
     * Lookup table of all registered link_receivers for this host,
     * keyed on their 24-bit magic control packet type.
     */
    std::unordered_map<magic_t, link_receiver*> receivers_;
    /**
     * List of all currently-active links.
     */
    std::unordered_set<link*> active_links_;
    /**
     * ipv4 link created by init_link(), if any.
     */
    std::shared_ptr<link> primary_link_;
    /**
     * ipv6 link created by init_link(), if any.
     */
    std::shared_ptr<link> primary_link6_;

protected:
    /**
     * Create a new network link.
     * The default implementation creates a udp_link,
     * but this may be overridden to virtualize the network.
     */
    virtual std::shared_ptr<link> create_link();

    /**
     * Initialize the link this host instance uses to communicate.
     * It exits the application via abort() if socket creation fails.
     * @param settings     Settings provider for port number. If not null, init_link() looks
     *                     for a 'port' key and uses it in place of the specified default
     *                     port if found. In any case, sets the 'port' key to the port
     *                     actually used.
     * @param default_port Default port number to bind to if 'port' key not found in @a settings.
     * @return the created link (during this or a previous call).
     */
    void init_link(settings_provider* settings,
        uint16_t default_port = stream_protocol::default_port);

public:
    /**
     * Create a receiver and bind it to control channel magic.
     */
    void bind_receiver(magic_t magic, link_receiver* receiver) {
        if (magic & 0xff000000) {
            throw "Invalid magic value for binding a receiver.";
        }
        receivers_.insert(std::make_pair(magic, receiver)); // @todo: Will NOT replace existing element.
    }

    void unbind_receiver(magic_t magic) {
        receivers_.erase(magic);
    }

    bool has_receiver_for(magic_t magic) {
        return contains(receivers_, magic);
    }

    /**
     * Find and return a receiver for given control channel magic value.
     */
    virtual link_receiver* receiver(magic_t magic);

    inline void activate_link(link* l)   { active_links_.insert(l); on_active_links_changed(); }
    inline void deactivate_link(link* l) { active_links_.erase(l); on_active_links_changed(); }

    /**
     * Obtain a list of all currently active links.
     * Used by upper-level protocols (e.g., key exchange, registration)
     * to send out initial discovery messages on all available links.
     * Subsequent messages normally get sent only to
     * the specific link a discovery response was seen on.
     * @return a set of pointers to each currently active link.
     */
    inline std::unordered_set<link*> active_links() const { return active_links_; }

    /**
     * Get a set of all known local endpoints for all active links.
     */
    std::unordered_set<endpoint> active_local_endpoints();

    typedef boost::signals2::signal<void(void)> active_links_changed_signal;
    /**
     * This signal is sent whenever the host's set of active sockets changes.
     */
    active_links_changed_signal on_active_links_changed;
};

/**
 * Abstract base class for entity connecting two endpoints using some network.
 * Link manages connection lifetime and maintains the connection status info.
 * For connected links there may be a number of channels established using their own keying schemes.
 * Link orchestrates initiation of key exchanges and scheme setup.
 */
class link //: public std::enable_shared_from_this<link>
{
    /**
     * Host state instance this link is attached to.
     */
    std::shared_ptr<host> host_;
    /**
     * Channels working through this link at the moment.
     */
    std::map<std::pair<endpoint, channel_number>, link_channel*> channels_;
    /**
     * True if this link is fair game for use by upper level protocols.
     */
    bool active_{false};

public:
    // ssu expresses current link status as one of three states:
    enum class status {
        down,    ///< definitely appears to be down.
        stalled, ///< briefly lost connectivity, but may be temporary.
        up       ///< apparently alive, all's well as far as we know.
    };

    static std::string status_string(status s);

    link(std::shared_ptr<host> host) : host_(host) {}
    virtual ~link();

    /**
     * Determine whether this link is active.
     * Only active link are returned by link_host_state::active_links().
     * @return true if link is active.
     */
    inline bool is_active() const { return active_; }

    /**
     * Activate or deactivate this link.
     * Only active link are returned by link_host_state::active_links().
     * @param active true if the link should be marked active.
     */
    void set_active(bool active);

    /**
     * Open the underlying socket, bind it to given endpoint and activate it if successful.
     * @param  ep Endpoint on the local machine to bind the link to.
     * @return    true if bind successfull, false otherwise.
     */
    virtual bool bind(endpoint const& ep) = 0;
    /**
     * Unbind and close the underlying socket.
     */
    virtual void unbind() = 0;

    /**
     * Send a packet on this link.
     * @param ep the destination address to send the packet to.
     * @param data the packet data.
     * @param size the packet size.
     * @return true if send was successful.
     */
    virtual bool send(endpoint const& ep, const char* data, size_t size) = 0;

    /**
     * Send a packet on this link.
     * This is an overridden function provided for convenience.
     * @param ep the destination address to send the packet to.
     * @param msg the packet data.
     * @return true if send was successful.
     */
    inline bool send(endpoint const& ep, byte_array const& msg) {
        return send(ep, msg.const_data(), msg.size());
    }

    /**
     * Find all known local endpoints referring to this link.
     * @return a list of endpoint objects.
     */
    virtual std::vector<endpoint> local_endpoints() = 0;
    /**
     * Return local port number at which this link is bound on the host.
     * @return local open port number.
     */
    virtual uint16_t local_port() = 0;

    /**
     * Return a description of any error detected on bind() or send().
     */
    virtual std::string error_string() = 0;

    /**
     * Find channel associations attached to this socket.
     */
    link_channel* channel_for(endpoint const& src, channel_number cn) {
        auto key = std::make_pair(src, cn);
        if (!contains(channels_, key))
            return nullptr;
        return channels_[key];
    }

    /**
     * Bind a new link_channel to this link.
     * Called by link_channel::bind() to register in the table of channels.
     */
    bool bind_channel(endpoint const& ep, channel_number chan, link_channel* lc);
    /**
     * Unbind a link_channel associated with endpoint @a ep and channel number @a chan.
     * Called by link_channel::unbind() to unregister from the table of channels.
     */
    void unbind_channel(endpoint const& ep, channel_number chan);

    /**
     * Returns true if this link provides congestion control
     * when communicating with the specified remote endpoint.
     */
    virtual bool is_congestion_controlled(endpoint const& ep);

    /**
     * For congestion-controlled links, returns the number of packets that may
     * be transmitted now to a particular target endpoint.
     */
    virtual int may_transmit(endpoint const& ep);

protected:
    /**
     * Implementation subclass calls this method with received packets.
     * @param msg the packet received.
     * @param src the source from which the packet arrived.
     */
    void receive(const byte_array& msg, const link_endpoint& src);

};

/**
 * Class for UDP connection between two endpoints.
 * Multiplexes between channel-setup/key exchange traffic (which goes to ssu::key_responder)
 * and per-channel data traffic (which goes to ssu::channel).
 */
class udp_link : public link
{
    /**
     * Underlying socket.
     */
    boost::asio::ip::udp::socket udp_socket;
    // boost::asio::ip::udp::socket udp6_socket; ///< ipv6 socket - host manages two udp_links instead
    boost::asio::streambuf received_buffer;
    /**
     * Endpoint we've received the packet from.
     */
    link_endpoint received_from;
    /**
     * Network activity execution queue.
     */
    boost::asio::strand strand_;
    /**
     * Socket error status.
     */
    std::string error_string_;

public:
    udp_link(std::shared_ptr<host> host);

    /**
     * Bind this UDP link to a port and activate it if successful.
     * @param  ep Local endpoint to bind to.
     * @return    true if bind successful and link has been activated, false otherwise.
     */
    bool bind(endpoint const& ep) override;
    void unbind() override;

    /**
     * Send a packet on this UDP link.
     * @param  ep   Target endpoint - intended receiver of the packet.
     * @param  data Packet data.
     * @param  size Packet size.
     * @return      If send was successful, i.e. the packet has been sent. It does not say anything
     *              about the reception of the packet on the other side, if it was ever delivered
     *              or accepted.
     */
    bool send(endpoint const& ep, char const* data, size_t size) override;
    using link::send;

    /**
     * Return a description of any error detected on bind() or send().
     */
    inline std::string error_string() override { return error_string_; }

    /**
     * Return all known local endpoints referring to this link.
     */
    std::vector<endpoint> local_endpoints() override;

    uint16_t local_port() override;

private:
    void prepare_async_receive();
    void udp_ready_read(const boost::system::error_code& error, std::size_t bytes_transferred);
};

/**
 * Helper function to bind a passed in socket to a given ep and set the error string to
 * occured error if any.
 * @param  sock         UDP socket to open and bind.
 * @param  ep           Endpoint to bind to. Can be ipv4 or ipv6.
 * @param  error_string Output string to set if error occured.
 * @return              true if successful, false if any error occured. Error string is set then.
 */
bool bind_socket(boost::asio::ip::udp::socket& sock, ssu::endpoint const& ep, std::string& error_string);

} // ssu namespace

namespace flurry {

// Flurry specialization for endpoint
inline flurry::oarchive& operator << (flurry::oarchive& oa, ssu::endpoint const& ep)
{
    if (ep.address().is_v4())
        oa << byte_array(ep.address().to_v4().to_bytes());
    else
        oa << byte_array(ep.address().to_v6().to_bytes());
    oa << ep.port();
    return oa;
}

inline flurry::iarchive& operator >> (flurry::iarchive& ia, ssu::endpoint& ep)
{
    byte_array addr;
    uint16_t port;
    ia >> addr >> port;
    if (addr.size() == std::tuple_size<boost::asio::ip::address_v6::bytes_type>::value) { // v6 address
        boost::asio::ip::address_v6 v6(addr.as<boost::asio::ip::address_v6::bytes_type>()[0]);
        ep = ssu::endpoint(v6, port);
    }
    else { // v4 address
        boost::asio::ip::address_v4 v4(addr.as<boost::asio::ip::address_v4::bytes_type>()[0]);
        ep = ssu::endpoint(v4, port);
    }
    return ia;
}

inline flurry::oarchive& operator << (flurry::oarchive& oa, ssu::link_endpoint const& ep)
{
    oa << (ssu::endpoint const&)ep;
    return oa;
}

inline flurry::iarchive& operator >> (flurry::iarchive& ia, ssu::link_endpoint& ep)
{
    ia >> (ssu::endpoint&)ep;
    return ia;
}

} // flurry namespace

// Hash specialization for endpoint
namespace std {

template<> 
struct hash<ssu::endpoint> : public std::unary_function<ssu::endpoint, size_t>
{
    inline size_t operator()(ssu::endpoint const& a) const noexcept
    {
        // this is a crappy slow hash until we can get ahold of a good c++1y hash
        return hash<std::string>()(a.address().to_string()) ^ (hash<int>()(a.port()) << 1);
    }
};

} // std namespace
