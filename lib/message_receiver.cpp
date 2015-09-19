#include "message_receiver.h"

socket_channel::weak_ptr
message_receiver::channel_for(string channel_key)
{
    if (!contains(channels_, channel_key)) {
        return socket_channel_ptr();
    }
    return channels_[channel_key];
}

bool
message_receiver::bind_channel(string channel_key, socket_channel::weak_ptr lc)
{
    assert(channel_for(channel_key).lock() == nullptr);
    channels_[channel_key] = lc;
    return true;
}

void
message_receiver::unbind_channel(string channel_key)
{
    channels_.erase(channel_key);
}

void
message_receiver::receive(byte_array const& msg, uia::comm::socket_endpoint const& src)
{
    if (auto channel = channel_for(msg.string_view(8, 32)).lock()) {
        return channel->receive(msg, src);
    }
}
