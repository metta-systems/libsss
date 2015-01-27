#include "packet_frame.h"

namespace sss { namespace framing {

using ack_frame_type_t = std::integral_constant<uint8_t, 2>;

}}

BOOST_FUSION_DEFINE_STRUCT(
    (sss)(framing), ack_frame_header_t,
    (sss::framing::ack_frame_type_t, type)
    (uint8_t, sent_entropy)
    (uint8_t, received_entropy)
    (uint8_t, num_missing)
    (uint64_t, least_unacked)
    (uint64_t, largest_observed)
    (uint32_t, largest_observed_deltatime)
); // @todo Use write_sizer to estimate header structure size

class ack_frame_t : public packet_frame_t
{
    void write(asio::mutable_buffer output,
               sss::framing::ack_frame_header_t hdr, string data);
    // data contains (possibly empty) array of 64 bit NACK entries
    void read(asio::const_buffer input);
};
