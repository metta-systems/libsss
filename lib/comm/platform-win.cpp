//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include <boost/asio.hpp>
#include <algorithm>
#include "arsenal/logging.h"
#include "comm/platform.h"
#include "arsenal/byte_array.h"

using namespace std;

namespace uia {
namespace comm {
namespace platform {

vector<endpoint> local_endpoints()
{
    boost::asio::udp::socket socket;
    if (!socket.bind())
        logger::fatal() << "Can't bind local UDP socket";

    SOCKET sockfd = socket.native_handle();
    assert(sockfd != INVALID_SOCKET);

    // Get the local host's interface list from Winsock via WSAIoctl().
    // Since we have no way of knowing how big a buffer we need,
    // retry with progressively larger buffers until we succeed.
    byte_array buf;
    buf.resize(sizeof(SOCKET_ADDRESS_LIST)*2);
    int retries = 0;
    while(true)
    {
        DWORD actsize;
        int rc = WSAIoctl(sockfd, SIO_ADDRESS_LIST_QUERY, NULL, 0,
                buf.data(), buf.size(), &actsize, NULL, NULL);
        if (rc == 0)
            break;

        buf.resize(buf.size() * 2);
        if (++retries > 20)
            logger::fatal() << "Can't find local host's IP addresses: " << WSAGetLastError();
    }

    // Parse the returned address list.
    vector<endpoint> result;
    SOCKET_ADDRESS_LIST *salist = (SOCKET_ADDRESS_LIST*)buf.data();
    boost::asio::ip::address address;

    for (int i = 0; i < salist->iAddressCount; i++)
    {
        sockaddr *sa = salist->Address[i].lpSockaddr;
        sockaddr_in* addr_in = (sockaddr_in*) salist->Address[i].lpSockaddr;

        if (sa->sa_family == AF_INET)
        {
            boost::asio::ip::address_v4::bytes_type bytes;
            copy((uint8_t*)&addr_in->sin_addr, (uint8_t*)(&addr_in->sin_addr) + bytes.size(), bytes.begin());
            boost::asio::ip::address_v4 addr(bytes);
            if (addr.is_loopback())
                continue;
            address = addr;
        }
        else if (sa->sa_family == AF_INET6)
        {
            boost::asio::ip::address_v6::bytes_type bytes;
            copy((uint8_t*)&addr_in->sin_addr, (uint8_t*)(&addr_in->sin_addr) + bytes.size(), bytes.begin());
            boost::asio::ip::address_v6 addr(bytes);
            if (addr.is_loopback() || addr.is_link_local())
                continue;
            address = addr;
        }

        // result.push_back(address);
        logger::debug() << "Local IP address: " << address;
    }

    return result;
}

} // platform namespace
} // comm namespace
} // uia namespace
