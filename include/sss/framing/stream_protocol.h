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
#include <type_traits>
#include "arsenal/byte_array.h"
#include "arsenal/opaque_endian.h"
#include "arsenal/flurry.h"
#include "arsenal/hash_combine.h"
#include "comm/host_interface.h"

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

    static constexpr size_t mtu = 1280; // an ipv6 frame size, not fragmentable
    static constexpr size_t min_receive_buffer_size = mtu * 2;

    // Maximum size of datagram to send using the stateless optimization.
    // @fixme Should be dynamic.
    // Datagram reassembly not yet supported, when done it could be around 2x-4x MTU size?
    static constexpr size_t max_stateless_datagram_size = mtu;

    // struct stream_header
    // {
    //     big_uint16_t stream_id;    // LSID
    //     uint8_t      type_subtype; // Field consists of two 4 bit fields - type and flags
    //     uint8_t      window;
    //  } __attribute__((packed));

    // struct init_header : public stream_header
    // {
    // 	big_uint16_t new_stream_id;
    // 	big_uint16_t tx_seq_no;
    // } __attribute__((packed));
    // using reply_header = init_header;
    // // init/reply_header and data_header must be the same size to allow optimized init_packets
    // struct data_header : public stream_header
    // {
    // 	big_uint32_t tx_seq_no;
    // } __attribute__((packed));
    // using datagram_header = stream_header;
    // using ack_header = stream_header;
    // using reset_header = stream_header;
    // using attach_header = stream_header;
    // using detach_header = stream_header;

    enum class frame_type : uint8_t
    {
        EMPTY = 0,
        STREAM = 1,
        ACK = 2,
        PADDING = 3,
        DECONGESTION = 4,
        DETACH = 5,
        RESET = 6,
        CLOSE = 7,
        SETTINGS = 8,
        PRIORITY = 9
    };

    /**
     * Major packet type codes (4 bits).
     *
     * @todo Remove this and use frame types in framing layer instead.
     */
    // enum class packet_type : uint8_t
    // {
    //     invalid  = 0x0, ///< Always invalid
    //     init     = 0x1, ///< Initiate new stream
    //     reply    = 0x2, ///< Reply to new stream
    //     data     = 0x3, ///< Regular data packet
    //     datagram = 0x4, ///< Best-effort datagram
    //     ack      = 0x5, ///< Explicit acknowledgment
    //     reset    = 0x6, ///< Reset stream
    //     attach   = 0x7, ///< Attach stream
    //     detach   = 0x8, ///< Detach stream
    //     /// 0x9-0xf are reserved for future extension and should not be used.
    // };
    /*
     * @todo Remove this and use frame types in framing layer instead.
     */
    enum flags : uint8_t
    {
        // Subtype/flag bits for init, reply, and data packets
        data_close        = 0x1,  ///< End of stream.
        data_record       = 0x2,  ///< End of record.
        data_push         = 0x4,  ///< Push to application. -- is this needed?
        data_all          = 0x7,  ///< All signal flags.

        // Flag bits for datagram packets
        datagram_begin    = 0x2,  ///< First fragment.
        datagram_end      = 0x1,  ///< Last fragment.

        // Flag bits for attach packets
        attach_init       = 0x8,  ///< Initiate stream.
        attach_slot_mask  = 0x1,  ///< Slot to use.

        // Flag bits for reset packets
        reset_remote_sid  = 0x1,  ///< SID orientation (set: sent LSID is in remote space)

        // The Window field consists of some flags and a 5-bit exponent.
        // @fixme Currently unused.
        window_substream  = 0x80, ///< Substream window
        window_inherit    = 0x40, ///< Inherited window
    };

    // static inline uint8_t type_and_subtype(packet_type type, uint8_t subtype) {
    //     return uint8_t(type) << 4 | (subtype & 0xf);
    // }

    // static inline packet_type type_from_header(stream_header const* hdr) {
    //     return packet_type(hdr->type_subtype >> 4);
    // }

    /// Service message codes
    enum service_code : uint32_t
    {
        connect_request        = 0x101, ///< Connect to named service.
        // request format: string service, string protocol
        connect_reply          = 0x201, ///< Response to connect request.
        // reply format: reply code, string description
        list_services_request  = 0x102, ///< Spec 4.4 end
        list_services_reply    = 0x202, ///< with human-readable descriptions
        /// reply format: array of pairs <service, service_desc>
        list_protocols_request = 0x103, ///< List protocols for given service
        /// request format: string service_name
        list_protocols_reply   = 0x203, ///< with descriptions
        /// reply format: array of pairs <protocol, protocol_desc>

        reply_ok         = 0,        ///< Service request accepted.
        reply_not_found  = 1,        ///< Specified service pair not found.
    };

    // Maximum size of a service request or response record
    static constexpr int max_service_record_size = 128; // @todo 1280 minus header sizes..
    // @fixme Could become bigger if list commands are implemented.
};

} // sss namespace
