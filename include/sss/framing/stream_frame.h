#include "packet_frame.h"

namespace sss { namespace framing {

using stream_frame_type_t = std::integral_constant<uint8_t, 1>;

}}

BOOST_FUSION_DEFINE_STRUCT(
    (sss)(framing), stream_frame_header_t,
    (sss::framing::stream_frame_type_t, type)
    (uint8_t, flags)
    (uint32_t, stream_id)
    (uint32_t, parent_stream_id)
    (usid_t, usid)
    (uint64_t, stream_offset)
    (uint16_t, data_length)
); // @todo Use sizer to estimate header structure size

class stream_frame_t : public packet_frame_t
{
    // This is obviously a horrible API, rework it into something more sensible.
    void write(asio::mutable_buffer output,
               stream_frame_header_t hdr, string data,
               bool no_ack = false, bool init = false, bool end = false, bool usid = false);
    void read(asio::const_buffer input);
};
