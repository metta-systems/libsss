#include "packet_frame.h"

class padding_frame_t : public packet_frame_t
{
    void write(asio::mutable_buffer output,
               sss::framing::padding_frame_header_t hdr);
    
    void read(asio::const_buffer input);
};
