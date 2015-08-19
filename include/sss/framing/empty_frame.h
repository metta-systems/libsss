#include "packet_frame.h"

class empty_frame_t : public packet_frame_t
{
    void write(asio::mutable_buffer output, empty_frame_header_t hdr);
    void read(asio::const_buffer input);
};
