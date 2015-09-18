//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <cstdint>
#include <cstddef>

namespace sss {

/**
 * Packet sequence number for packets within a channel.
 */
using packet_seq_t = uint64_t;

constexpr size_t MIN_PACKET_SIZE = 64;

/**
 * SSS stream protocol definitions.
 * This class simply provides SSS protcol definition constants for use in other stream classes.
 */
class stream_protocol
{
public:
    static constexpr uint16_t default_port = 9660;

    static constexpr size_t mtu                     = 1280; // an ipv6 frame size, not fragmentable
    static constexpr size_t min_receive_buffer_size = mtu * 2; // @todo Not needed?

    enum class frame_type : uint8_t
    {
        EMPTY        = 0,
        STREAM       = 1,
        ACK          = 2,
        PADDING      = 3,
        DECONGESTION = 4,
        DETACH       = 5,
        RESET        = 6,
        CLOSE        = 7,
        SETTINGS     = 8,
        PRIORITY     = 9
    };

    /// Service message codes
    /// These are sent on stream LSID 0
    enum service_code : uint32_t
    {
        connect_request = 0x101, ///< Connect to named service.
        // request format: string service, string protocol
        connect_reply = 0x201, ///< Response to connect request.
        // reply format: reply code, string description
        list_services_request = 0x102, ///< Spec 4.4 end
        list_services_reply   = 0x202, ///< with human-readable descriptions
        /// reply format: array of pairs <service, service_desc>
        list_protocols_request = 0x103, ///< List protocols for given service
        /// request format: string service_name
        list_protocols_reply = 0x203, ///< with descriptions
        /// reply format: array of pairs <protocol, protocol_desc>

        reply_ok        = 0, ///< Service request accepted.
        reply_not_found = 1, ///< Specified service pair not found.
    };

    // Maximum size of a service request or response record
    static constexpr int max_service_record_size = 128; // @todo 1280 minus header sizes..
    // @fixme Could become bigger if list commands are implemented.
};

} // sss namespace
