//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2015, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include "sss/framing/framing_types.h"

//=================================================================================================
// Framing layer
//=================================================================================================

namespace sss { namespace framing {

struct uint24_t {
    uint16_t high;
    uint8_t  low;
    operator uint64_t() { return uint64_t(high) << 8 | low; }
};

struct uint40_t {
    uint32_t high;
    uint8_t  low;
    operator uint64_t() { return uint64_t(high) << 8 | low; }
};

struct uint48_t {
    uint32_t high;
    uint16_t low;
    operator uint64_t() { return uint64_t(high) << 16 | low; }
};

struct uint56_t {
    uint32_t high;
    uint24_t low;
    operator uint64_t() { return uint64_t(high) << 24 | low; }
};

}}

BOOST_FUSION_ADAPT_STRUCT(
    sss::framing::uint24_t,
    (uint32_t, high)
    (uint8_t, low)
);

BOOST_FUSION_ADAPT_STRUCT(
    sss::framing::uint40_t,
    (uint32_t, high)
    (uint8_t, low)
);

BOOST_FUSION_ADAPT_STRUCT(
    sss::framing::uint48_t,
    (uint32_t, high)
    (uint16_t, low)
);

BOOST_FUSION_ADAPT_STRUCT(
    sss::framing::uint56_t,
    (uint32_t, high)
    (sss::framing::uint24_t, low)
);


BOOST_FUSION_DEFINE_STRUCT(
    (sss)(framing), packet_sequence_number,
    (uint16_t, size2)
    (uint32_t, size4)
    (sss::framing::uint48_t, size6)
    (uint64_t, size8)
);

// namespace sss::framing { // -std=c++1z with SVN clang
namespace sss { namespace framing {

using packet_flag_field_t = field_flag<uint8_t>;
using version_field_t = optional_field_specification<uint16_t, field_index<0>, 0_bits_shift>;
using fec_field_t = optional_field_specification<uint8_t, field_index<0>, 1_bits_shift>;
using packet_size_t = varsize_field_wrapper<packet_sequence_number, uint64_t>;
using packet_field_t = varsize_field_specification<packet_size_t, field_index<0>,
    2_bits_mask, 2_bits_shift>;

}}

BOOST_FUSION_DEFINE_STRUCT( // done: r
    (sss)(framing), packet_header,
    (sss::framing::packet_flag_field_t, flags) // 000fssgv
    (sss::framing::version_field_t, version)
    (sss::framing::fec_field_t, fec_group)
    (sss::framing::packet_field_t, packet_sequence)
);

//-------------------------------------------------------------------------------------------------
// STREAM frame
//-------------------------------------------------------------------------------------------------

BOOST_FUSION_DEFINE_STRUCT(
    (sss)(framing), packet_stream_offset,
    (nothing_t,              size0)
    (uint16_t,               size2)
    (sss::framing::uint24_t, size3)
    (uint32_t,               size4)
    (sss::framing::uint40_t, size5)
    (sss::framing::uint48_t, size6)
    (sss::framing::uint56_t, size7)
    (uint64_t,               size8)
);

template <typename SizeFieldIndex>
struct ext_sized_field_t
{
    std::string data;
};

namespace sss { namespace framing {

using stream_frame_type_t = std::integral_constant<uint8_t, 1>;
using stream_flags_field_t = field_flag<uint8_t>;
using optional_parent_sid_t = optional_field_specification<uint32_t, field_index<1>, 6_bits_shift>;
using optional_usid_t = optional_field_specification<usid_t, field_index<1>, 5_bits_shift>;
using optional_data_length_t = optional_field_specification<uint16_t, field_index<1>, 1_bits_shift>;
using stream_offset_t = varsize_field_wrapper<packet_stream_offset, uint64_t>;
using packet_stream_offset_t = varsize_field_specification<stream_offset_t, field_index<1>,
    3_bits_mask, 2_bits_shift>;
using frame_data_t = ext_sized_field_t<field_index<6>>;

}}

BOOST_FUSION_DEFINE_STRUCT(
    (sss)(framing), stream_frame_header,
    (sss::framing::stream_frame_type_t, type)
    (sss::framing::stream_flags_field_t, flags)
    (uint32_t, stream_id)
    (sss::framing::optional_parent_sid_t, parent_stream_id)
    (sss::framing::optional_usid_t, usid)
    (sss::framing::packet_stream_offset_t, stream_offset)
    (sss::framing::optional_data_length_t, data_length)
    (sss::framing::frame_data_t, frame) // variable size data
    // ^^ ext length spec through data_length member
);

BOOST_FUSION_DEFINE_STRUCT(
    (sss)(framing), ack_frame_header,
    (uint8_t, type)
    (uint8_t, sent_entropy)
    (uint8_t, received_entropy)
    (uint8_t, missing_packets)
    (uint64_t, least_unacked_packet)
    (uint64_t, largest_observed_packet)
    (uint32_t, largest_observed_delta_time)
    (std::vector<uint64_t>, nacks)
);

BOOST_FUSION_DEFINE_STRUCT(
    (sss)(framing), padding_frame_header,
    (uint8_t, type)
    (uint16_t, length)
    (rest_t, frame) // [length] padding data - @todo only until end of the frame! ext length spec...
);

BOOST_FUSION_DEFINE_STRUCT(
    (sss)(framing), decongestion_frame_header,
    (uint8_t, type)
);

BOOST_FUSION_DEFINE_STRUCT(
    (sss)(framing), detach_frame_header,
    (uint8_t, type)
);

BOOST_FUSION_DEFINE_STRUCT(
    (sss)(framing), reset_frame_header,
    (uint8_t, type)
);

BOOST_FUSION_DEFINE_STRUCT(
    (sss)(framing), close_frame_header,
    (uint8_t, type)
);

BOOST_FUSION_DEFINE_STRUCT(
    (sss)(framing), settings_frame_header,
    (uint8_t, type)
);

BOOST_FUSION_DEFINE_STRUCT(
    (sss)(framing), priority_frame_header,
    (uint8_t, type)
);

