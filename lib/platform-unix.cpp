//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <boost/asio.hpp>
#include <algorithm>
#include "logging.h"
#include "platform.h"

using namespace std;

namespace ssu {
namespace platform {

std::string user_name()
{
    char const* username = getenv("USER");
    if (username == nullptr)
        username = getenv("LOGNAME");
    if (username == nullptr)
        username = getlogin();
    if (username == nullptr)
        username = "Someone";
    return std::string(username);
}

#define NO_PORT 0

// http://vrjuggler.org/docs/vapor/2.2/programmer.reference/namespacevpr.html#aadd07b8751f2d2ba6b757e9c11fd7eab
std::vector<ssu::endpoint> local_endpoints()
{
    std::vector<ssu::endpoint> result;

    ifaddrs *ifa{nullptr};
    if (getifaddrs(&ifa) < 0) {
        logger::warning() << "Can't find my own IP addresses!?";
        return result;
    }

    for (ifaddrs *a = ifa; a; a = a->ifa_next) {
        sockaddr* sa = a->ifa_addr;
        sockaddr_in* addr_in = (sockaddr_in*) a->ifa_addr;

        if (sa
            and ((sa->sa_family == AF_INET) or (sa->sa_family == AF_INET6)) 
            and (a->ifa_flags & IFF_UP))
        {
            if (sa->sa_family == AF_INET)
            {
                boost::asio::ip::address_v4::bytes_type bytes;
                copy((uint8_t*)&addr_in->sin_addr, (uint8_t*)(&addr_in->sin_addr) + bytes.size(), bytes.begin());
                boost::asio::ip::address_v4 address(bytes);
                if (address.is_loopback() or address.is_unspecified())
                    continue;
                result.emplace_back(address, NO_PORT);
                logger::debug() << "Local IP address: " << address;
            }
            else if (sa->sa_family == AF_INET6)
            {
                boost::asio::ip::address_v6::bytes_type bytes;
                copy((uint8_t*)&addr_in->sin_addr, (uint8_t*)(&addr_in->sin_addr) + bytes.size(), bytes.begin());
                boost::asio::ip::address_v6 address(bytes);
                if (address.is_loopback() or address.is_link_local() or address.is_unspecified())
                    continue;
                result.emplace_back(address, NO_PORT);
                logger::debug() << "Local IP address: " << address;
            }
        }
    }

    freeifaddrs(ifa);

    return result;
}

} // platform namespace
} // ssu namespace
