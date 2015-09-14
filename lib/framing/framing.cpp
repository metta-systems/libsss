#include "sss/framing/framing.h"

#include "sss/framing/ack_frame.h"
#include "sss/framing/close_frame.h"
#include "sss/framing/decongestion_frame.h"
#include "sss/framing/detach_frame.h"
#include "sss/framing/empty_frame.h"
#include "sss/framing/frame_format.h"
#include "sss/framing/padding_frame.h"
#include "sss/framing/priority_frame.h"
#include "sss/framing/reset_frame.h"
#include "sss/framing/settings_frame.h"
#include "sss/framing/stream_frame.h"

namespace sss { namespace framing {

namespace {

template <typename T>
class has_dispatch {
    template <typename U>
    static char check(decltype(static_cast<U*>(nullptr)->
                dispatch(static_cast<channel::ptr>(nullptr)))*);

    template <typename U>
    static int check(...);

public:
    static constexpr bool value = (sizeof(check<T>(nullptr)) == sizeof(char));
};

template<typename T, bool c>
struct dispatch_caller__
{
public:
    static void call(T&, channel::ptr)
    {
    }
};

template <typename T>
struct dispatch_caller__<T, true>
{
public:
    static void call(T& o, channel::ptr c)
    {
        o.dispatch(c);
    }
};

template <typename T>
using dispatch_caller = dispatch_caller__<T, has_dispatch<T>::value>;

}

template <typename T>
void framing_t::read_handler(boost::asio::const_buffer input)
{
    T frame;
    frame.read(input);
    dispatch_caller<T>::call(frame, channel_);
}

std::array<framing_t::read_handler_type, max_frame_count_t::value> framing_t::handlers_ = { {
    &framing_t::read_handler<empty_frame_t>,
    &framing_t::read_handler<stream_frame_t>,
    &framing_t::read_handler<ack_frame_t>,
    &framing_t::read_handler<padding_frame_t>,
    &framing_t::read_handler<decongestion_frame_t>,
    &framing_t::read_handler<detach_frame_t>,
    &framing_t::read_handler<reset_frame_t>,
    &framing_t::read_handler<close_frame_t>,
    &framing_t::read_handler<settings_frame_t>,
    &framing_t::read_handler<priority_frame_t>
} };

framing_t::framing_t(channel::ptr c)
    : channel_{c}
{
}

void framing_t::enframe(boost::asio::mutable_buffer output)
{
    /*
    if (sizer::estimate_size(packets.front()) < boost::asio::buffer_size(output_buffer)) {
        write(output_buffer, packets.front());
        packets.pop();
    }
    if (boost::asio::buffer_size(output_buffer) > 0) {
        filler(output_buffer);
    }
    */
}

// Read packet frames and deliver decoded frames to appropriate handlers.
void framing_t::deframe(boost::asio::const_buffer input)
{
    while (boost::asio::buffer_size(input) > 0) {
        uint8_t frame_type = *boost::asio::buffer_cast<const uint8_t*>(input);
        if (frame_type >= max_frame_count_t::value) throw "Invalid frame type";
        (this->*handlers_[frame_type])(input);
    }
}

}

}

