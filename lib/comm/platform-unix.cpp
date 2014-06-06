//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@atta-metta.net>
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
#include "arsenal/logging.h"
#include "comm/platform.h"

using namespace std;

namespace uia {
namespace comm {
namespace platform {

#define NO_PORT 0

// http://vrjuggler.org/docs/vapor/2.2/programmer.reference/namespacevpr.html#aadd07b8751f2d2ba6b757e9c11fd7eab
vector<endpoint> local_endpoints()
{
    vector<endpoint> result;

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
} // comm namespace
} // uia namespace
