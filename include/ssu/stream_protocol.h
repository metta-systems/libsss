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

namespace ssu {

/**
 * Packet sequence numbers are 64-bit unsigned integers.
 */
typedef uint64_t packet_seq_t;

static constexpr packet_seq_t max_packet_seq = ~0ULL;

/**
 * SSU stream protocol definitions.
 * This class simply provides SSU protcol definition constants
 * for use in the other stream classes.
 */
class stream_protocol
{
public:
    static constexpr uint16_t default_port = 9660;

    static constexpr size_t mtu = 1200; // @fixme This needs to be a per-link variable.
    static constexpr size_t min_receive_buffer_size = mtu * 2; // @fixme Should be dynamic based on mtu.

    // Maximum size of datagram to send using the stateless optimization.
    // @fixme Should be dynamic.
    // Datagram reassembly not yet supported, when done it could be around 2x-4x MTU size?
    static constexpr size_t max_stateless_datagram_size = mtu;

    // Control chunk magic value for the structured streams.
    // Top byte is channel number and must be zero.
    // 0x535355 = 'SSU': 'Structured Streams Unleashed'
    static constexpr uia::comm::magic_t magic_id = 0x00535355;

    typedef uint64_t counter_t;    ///< Counter for SID assignment.
    typedef uint16_t stream_id_t;  ///< Stream ID within channel.

    struct stream_header
    {
        big_uint16_t stream_id;
        uint8_t      type_subtype; // Field consists of two 4 bit fields - type and flags
        uint8_t      window;
    } __attribute__((packed));

    struct init_header : public stream_header
    {
    	big_uint16_t new_stream_id;
    	big_uint16_t tx_seq_no;
    } __attribute__((packed));
    typedef init_header reply_header;
    // init/reply_header and data_header must be the same size to allow optimized init_packets
    struct data_header : public stream_header
    {
    	big_uint32_t tx_seq_no;
    } __attribute__((packed));
    typedef stream_header datagram_header;
    typedef stream_header ack_header;
    typedef stream_header reset_header;
    typedef stream_header attach_header;
    typedef stream_header detach_header;

    /**
     * Major packet type codes (4 bits).
     */
    enum class packet_type : uint8_t
    {
        invalid  = 0x0, ///< Always invalid
        init     = 0x1, ///< Initiate new stream
        reply    = 0x2, ///< Reply to new stream
        data     = 0x3, ///< Regular data packet
        datagram = 0x4, ///< Best-effort datagram
        ack      = 0x5, ///< Explicit acknowledgment
        reset    = 0x6, ///< Reset stream
        attach   = 0x7, ///< Attach stream
        detach   = 0x8, ///< Detach stream
        /// 0x9-0xf are reserved for future extension and should not be used.
    };

    enum flags : uint8_t
    {
        // Subtype/flag bits for init, reply, and data packets
        data_close        = 0x1,  ///< End of stream.
        data_record       = 0x2,  ///< End of record.
        data_push         = 0x4,  ///< Push to application.
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

    static inline uint8_t type_and_subtype(packet_type type, uint8_t subtype) {
        return uint8_t(type) << 4 | (subtype & 0xf);
    }

    static inline packet_type type_from_header(stream_header const* hdr) {
        return packet_type(hdr->type_subtype >> 4);
    }

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
    static constexpr int max_service_record_size = 128;
    // @fixme Could become bigger if list commands are implemented.

    /**
     * Type for identifying streams uniquely across channels.
     *
     * XXX should contain a "keying method identifier" of some kind?
     */
    struct unique_stream_id_t
    {
        counter_t counter_{0}; ///< Stream counter in channel
        byte_array half_channel_id_; ///< Unique channel+direction ID ("half-channel id")

        inline unique_stream_id_t() = default;

        inline unique_stream_id_t(counter_t counter, byte_array chan_id)
            : counter_(counter)
            , half_channel_id_(chan_id)
        {}

        inline bool is_empty() const {
            return half_channel_id_.is_empty();
        }

        inline bool operator ==(unique_stream_id_t const& other) const {
            return counter_ == other.counter_ and half_channel_id_ == other.half_channel_id_;
        }
    };
};

inline
std::ostream& operator << (std::ostream& os, stream_protocol::unique_stream_id_t const& id)
{
    os << "USID[" << id.counter_ << ":" << id.half_channel_id_ << "]";
    return os;
}

inline
flurry::oarchive& operator << (flurry::oarchive& oa, stream_protocol::unique_stream_id_t const& id)
{
    oa << id.counter_ << id.half_channel_id_;
    return oa;
}

inline
flurry::iarchive& operator >> (flurry::iarchive& ia, stream_protocol::unique_stream_id_t& id)
{
    ia >> id.counter_ >> id.half_channel_id_;
    return ia;
}

} // namespace ssu

// Hash specialization for unique_stream_id_t
namespace std {

template<>
struct hash<ssu::stream_protocol::unique_stream_id_t> : public std::unary_function<ssu::stream_protocol::unique_stream_id_t, size_t>
{
    inline size_t operator()(ssu::stream_protocol::unique_stream_id_t const& a) const noexcept
    {
        // VEEERY bad implementation for now. @fixme
        size_t seed = 0xdeadbeef;
        stdext::hash_combine(seed, a.counter_);
        stdext::hash_combine(seed, a.half_channel_id_);
        return seed;
    }
};

} // namespace std
