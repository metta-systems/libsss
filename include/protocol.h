//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <cstdint>
#include <type_traits>
#include "byte_array.h"
#include "opaque_endian.h"
#include "flurry.h"

namespace ssu {

/**
 * Protocol magic marker, must have 0x00 as the highest byte (channel number).
 */
typedef uint32_t magic_t;

/**
 * An 8-bit channel number distinguishes different channels
 * between the same pair of socket-layer endpoints. Channel number 0 is always invalid.
 * Up to 255 simultaneous channels possible.
 */
typedef uint8_t channel_number;

/**
 * Packet sequence number.
 */
typedef uint64_t packet_seq_t;

/**
 * @internal
 * SSU stream protocol definitions.
 * This class simply provides SSU protcol definition constants
 * for use in the other stream classes.
 */
class stream_protocol
{
public:
    static constexpr size_t mtu = 1500; // @fixme This needs to be a per-link variable.

    // Control chunk magic value for the structured streams.
    // 0x535355 = 'SSU': 'Structured Streams Unleashed'
    static constexpr magic_t magic_id = 0x00535355;

    typedef uint64_t counter_t;    ///< Counter for SID assignment.
    typedef uint16_t stream_id_t;  ///< Stream ID within channel.
    typedef uint32_t byteseq_t;    ///< Stream byte sequence number.

    enum class packet_type : uint8_t {
    	invalid  = 0x0,
    	init     = 0x1,
    	reply    = 0x2,
    	data     = 0x3,
    	datagram = 0x4,
    	ack      = 0x5,
    	reset    = 0x6,
    	attach   = 0x7,
    	detach   = 0x8,
    };

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
    struct data_header : public stream_header
    {
    	big_uint32_t tx_seq_no;
    } __attribute__((packed));
    typedef stream_header datagram_header;
    typedef stream_header ack_header;
    typedef stream_header reset_header;
    typedef stream_header attach_header;
    typedef stream_header detach_header;

    enum flags : uint8_t
    {
        // Subtype/flag bits for Init, Reply, and Data packets
        data_close        = 0x1,  ///< End of stream.
        data_message      = 0x2,  ///< End of message.
        data_push         = 0x4,  ///< Push to application.
        data_all          = 0x7,  ///< All signal flags.

        // Flag bits for Datagram packets
        dgram_begin       = 0x2,  ///< First fragment.
        dgram_end         = 0x1,  ///< Last fragment.

        // Flag bits for Attach packets
        attach_init       = 0x8,  ///< Initiate stream.
        attach_slot_mask  = 0x1,  ///< Slot to use.

        // Flag bits for Reset packets
        reset_remote      = 0x1,  ///< SID orientation (set: sent LSID is in remote space)
    };

    inline uint8_t type_and_subtype(packet_type type, uint8_t subtype) {
        return uint8_t(type) << 4 | (subtype & 0xf);
    }

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

        inline bool operator ==(unique_stream_id_t const& other) const
        {
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
