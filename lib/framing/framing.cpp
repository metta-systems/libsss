#include "stream_frame.h"
#include "ack_frame.h"

void framing_t::enframe()
{
    if (sizer::estimate_size(packets.front()) < buffer_size(output_buffer)) {
        write(output_buffer, packets.front());
        packets.pop();
    }
    if (buffer_size(output_buffer) > 0) {
        filler(output_buffer);
    }
}

// Read packet frames and deliver decoded frames to appropriate handlers.
void framing_t::deframe()
{
    // input buffer must be positioned at first frame
    uint8_t type = *buffer_cast<uint8_t*>(input_buffer);
    if (type > lastSupportedType) throw "Invalid frame type";
    switch (type) {
        case stream_frame_type_t::value:
            stream_frame_header_t hdr;
            read(input_buffer, hdr);
            STREAM_read_handler(hdr);
            break;
        case ack_frame_type_t::value:
            ack_frame_header_t hdr;
            read(input_buffer, hdr);
            ACK_read_handler(hdr);
            break;
        default:
            throw "Unreachable";
    }
}

// We've received a stream frame, dispatch it to corresponding awaiting stream or spawn a new one.
void framing_t::STREAM_read_handler(stream_frame_header_t hdr)
{}

// We've received ack frame, update missed packets information queues.
void framing_t::ACK_read_handler(ack_frame_header_t hdr)
{}
