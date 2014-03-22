//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "ssu/identity.h"
#include "krypto/sha256_hash.h"
#include "krypto/dsa160_key.h"
#include "krypto/rsa160_key.h"
#include "arsenal/logging.h"
#include "arsenal/settings_provider.h"

using namespace std;

namespace ssu {

//=================================================================================================
// identity
//=================================================================================================

identity::identity(byte_array const& id)
    : id_(id)
{
}

identity::identity(peer_id const& id)
    : id_(id)
{}

identity::identity(byte_array const& id, byte_array const& key)
    : id_(id)
{
    if (!set_key(key)) {
        throw bad_key();
    }
}

void identity::clear_key()
{
    key_.reset();
}

bool identity::set_key(byte_array const& key)
{
    clear_key();

    scheme ksch = key_scheme();
    switch (ksch) {
        case dsa160:
            key_ = make_shared<crypto::dsa160_key>(key);
            break;
        case rsa160:
            key_ = make_shared<crypto::rsa160_key>(key);
            break;
        default:
            logger::warning() << "Unknown identity key scheme " << ksch;
            return false;
    }

    // Check if decode succeeded.
    if (key_->type() == crypto::sign_key::key_type::invalid)
    {
        clear_key();
        return false;
    }

    // Verify that the supplied key actually matches the ID we have.
    // *** This is a crucial step for security! ***
    byte_array key_id = key_->id();
    key_id[0] = (key_id[0] & 7) | (ksch << 3); // replace top 5 bits of ID with scheme used

    if (key_id != id_)
    {
        clear_key();
        logger::warning() << "Attempt to set mismatching identity key!";
        return false;
    }

    return true;
}

identity identity::generate(scheme sch, int bits)
{
    shared_ptr<crypto::sign_key> key{nullptr};
    switch (sch) {
        case dsa160:
            logger::debug() << "Generating new DSA160 sign key";
            key = make_shared<crypto::dsa160_key>(bits);
            break;
        case rsa160:
            logger::debug() << "Generating new RSA160 sign key";
            key = make_shared<crypto::rsa160_key>(bits);
            break;
        default:
            logger::fatal() << "Unsupported signing scheme " << sch;
    }

    byte_array id = key->id();
    id[0] = (id[0] & 7) | (sch << 3); // replace top 5 bits of ID with scheme used
    logger::debug() << "Generated key id " << id;

    identity ident(id);
    ident.key_ = key;

    return std::move(ident);
}

identity::scheme identity::key_scheme() const
{
    if(id_.is_empty())
        return scheme::null;

    return scheme(id_.id()[0] >> 3);
}

std::string identity::scheme_name() const
{
    switch (key_scheme())
    {
        case null: return "null";
        case mac:  return "mac";
        case ipv4: return "ipv4";
        case ipv6: return "ipv6";
        case dsa160: return "dsa160";
        case rsa160: return "rsa160";
        default:     return "unknown";
    }
}

identity identity::from_mac_address(byte_array const& mac)
{
    assert(mac.size() == 6);

    byte_array id({scheme::mac << 3});
    id.append(mac);
    return move(identity(id));
}

byte_array identity::mac_address() const
{
    if (key_scheme() != scheme::mac or id_.id().size() != 1+6)
        return byte_array();
    return id_.id().mid(1);
}

identity identity::from_ip_address(boost::asio::ip::address const& addr, uint16_t port)
{
    byte_array buf;

    if (addr.is_v4()) {
        buf.append({scheme::ipv4 << 3});
        buf.append(addr.to_v4().to_bytes());
    }
    else if (addr.is_v6()) {
        buf.append({scheme::ipv6 << 3});
        buf.append(addr.to_v6().to_bytes());
    }
    else {
        logger::warning() << "identity.from_ip_address - unknown IP protocol specified!";
        return std::move(identity()); // Unknown IP type
    }

    if (port)
    {
        big_uint16_t bport(port);
        buf.append(byte_array::wrap(reinterpret_cast<char const*>(&bport), 2));
    }

    return std::move(identity(buf));
}

boost::asio::ip::address identity::ip_address(uint16_t* out_port) const
{
    if (out_port) {
        *out_port = 0;
    }
    if (id_.is_empty()) {
        return boost::asio::ip::address();
    }

    boost::asio::ip::address address;
    size_t port_offset = ~0;

    switch (key_scheme())
    {
        case scheme::ipv4:
            if (id_.size() < 1+4) {
                return boost::asio::ip::address();
            }
            address = boost::asio::ip::address_v4(id_.id().mid(1,4).as<big_uint32_t>()[0]);
            port_offset = 1+4;
            break;
        case scheme::ipv6: {
            if (id_.size() < 1+16) {
                return boost::asio::ip::address();
            }
            boost::asio::ip::address_v6::bytes_type bytes;
            byte_array sub(id_.id().mid(1,16));
            std::copy(sub.as_vector().begin(), sub.as_vector().end(), bytes.begin());
            address = boost::asio::ip::address_v6(bytes);
            port_offset = 1+16;
            break;
        }
        default:
            logger::warning() << "identity.ip_address - unknown IP protocol!";
            return boost::asio::ip::address();
    }

    if (out_port and id_.size() >= port_offset+2) {
        *out_port = id_.id().mid(port_offset, 2).as<big_uint16_t>()[0];
    }

    return address;
}

uint16_t identity::ip_port() const
{
    uint16_t port;
    ip_address(&port);
    return port;
}

identity identity::from_endpoint(uia::comm::endpoint const& ep)
{
    return from_ip_address(ep.address(), ep.port());
}

uia::comm::endpoint identity::get_endpoint() const
{
    return uia::comm::endpoint(ip_address(), ip_port());
}

byte_array identity::public_key() const
{
    if (!key_)
        return byte_array();
    return key_->public_key();
}

byte_array identity::private_key() const
{
    if (!key_)
        return byte_array();
    return key_->private_key();
}

byte_array identity::hash(char const* data, int len) const
{
    return crypto::sha256::hash(data, len);
}

//=================================================================================================
// identity_host_state
//=================================================================================================

identity identity_host_state::host_identity()
{
    if (!host_identity_.has_private_key())
    {
        host_identity_ = identity::generate();
    }
    return host_identity_;
}

void identity_host_state::set_host_identity(identity const& ident)
{
    if (ident.has_private_key()) {
        logger::warning() << "Using a host identity with no private key!";
    }
    host_identity_ = ident;
}

void identity_host_state::init_identity(settings_provider* settings)
{
    if (host_identity_.has_private_key())
        return; // Already initialized.

    if (!settings)
    {
        host_identity(); // No persistence available.
        return;
    }

    // Find and decode the host's existing key, if any.
    byte_array id = settings->get_byte_array("id");
    byte_array key = settings->get_byte_array("key");

    if (!id.is_empty() and !key.is_empty())
    {
        host_identity_.set_id(id);
        if (host_identity_.set_key(key) && host_identity_.has_private_key())
            return;     // Success
    }

    logger::warning() << "Invalid host identity in settings: generating new identity";

    // Generate a new key pair
    host_identity_ = identity::generate();

    // Save it in our host settings
    settings->set("id", host_identity_.id().id().as_vector());
    settings->set("key", host_identity_.private_key().as_vector());
    settings->sync();
}

} // ssu namespace
