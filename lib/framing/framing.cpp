#include "framing.h"

#include "ack_frame.h"
#include "close_frame.h"
#include "decongestion_frame.h"
#include "detach_frame.h"
#include "empty_frame.h"
#include "frame_format.h"
#include "padding_frame.h"
#include "priority_frame.h"
#include "reset_frame.h"
#include "settings_frame.h"
#include "stream_frame.h"

namespace sss { namespace framing {

std::array<framing_t::read_handler_type, max_frame_count_type_t::value> framing_t::handlers_ = {
    framing_t::EMPTY_read_handler,
    framing_t::STREAM_read_handler,
    framing_t::ACK_read_handler,
    framing_t::PADDING_read_handler,
    framing_t::DECONGESTION_read_handler,
    framing_t::DETACH_read_handler,
    framing_t::RESET_read_handler,
    framing_t::CLOSE_read_handler,
    framing_t::SETTINGS_read_handler,
    framing_t::PRIORITY_read_handler
};


void framing_t::enframe(asio::mutable_buffer output)
{
    if (sizer::estimate_size(packets.front()) < asio::buffer_size(output_buffer)) {
        write(output_buffer, packets.front());
        packets.pop();
    }
    if (asio::buffer_size(output_buffer) > 0) {
        filler(output_buffer);
    }
}

// Read packet frames and deliver decoded frames to appropriate handlers.
void framing_t::deframe(asio::const_buffer input)
{
    while (asio::buffer_size(input) > 0) {
        uint8_t frame_type = *buffer_cast<uint8_t*>(input);
        if (type >= max_frame_count_type_t::value) throw "Invalid frame type";
        this->(*handlers_[type])(input);
    }
}

void framing_t::EMPTY_read_handler(asio::const_buffer input)
{
    empty_frame frame;
    frame.read(input);
}

void framing_t::STREAM_read_handler(asio::const_buffer input)
{
    stream_frame frame;
    frame.read(input);
    frame.dispatch(this);
}

void framing_t::ACK_read_handler(asio::const_buffer input)
{
    ack_frame frame;
    frame.read(input);
    frame.dispatch(this);
}

void framing_t::PADDING_read_handler(asio::const_buffer input)
{
    padding_frame frame();
    frame.read(input); 
}

void framing_t::DECONGESTION_read_handler(asio::const_buffer input)
{
    decongestion_frame frame();
    frame.read(input); 
    frame.dispatch(this);
}

void framing_t::DETACH_read_handler(asio::const_buffer input)
{
    detach_frame frame;
    frame.read(input);
    frame.dispatch(*this);
}

void framing_t::RESET_read_handler(asio::const_buffer input)
{
    reset_frame frame();
    frame.read(input);
    frame.dispatch(*this);
}

void framing_t::CLOSE_read_handler(asio::const_buffer input)
{
    close_frame frame();
    frame.read(input);
    frame.dispatch(*this);
}

void framing_t::SETTINGS_read_handler(asio::const_buffer input)
{
    settings_frame frame();
    frame.read(input);
    frame.dispatch(*this);
}

void framing_t::PRIORITY_read_handler(asio::const_buffer input)
{
    priority_frame frame();
    frame.read(input);
    frame.dispatch(*this);
}

} }
