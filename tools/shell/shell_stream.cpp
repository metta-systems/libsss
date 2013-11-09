#include "shell_stream.h"
#include "logging.h"

shell_stream::shell_stream(std::shared_ptr<ssu::stream> stream)
    : rstate(RecvNormal)
{
    if (stream)
        set_stream(stream);
}

void shell_stream::set_stream(std::shared_ptr<ssu::stream> stream)
{
    assert(stream_ == nullptr);
    stream_ = stream;

    stream_->on_ready_read.connect([this]{on_ready_read();});
    stream_->on_bytes_written.connect([this](ssize_t bytes){on_bytes_written(bytes);});
}

void shell_stream::send_data(const char *data, int size)
{
    // Escape any SOH characters in the stream
    const char *mark = (const char*)memchr(data, ControlMarker, size);
    while (mark != NULL) {
        static const char lenbyte = 0x80;
        int amt = mark+1-data;
        stream_->write_data(data, amt);
        stream_->write_data(&lenbyte, 1);
        data += amt;
        size -= amt;
        mark = (const char*)memchr(data, ControlMarker, size);
    }

    // Remainder contains no control markers
    stream_->write_data(data, size);
}

// Insert a control message into the outgoing character data stream.
void shell_stream::send_control(byte_array const& msg)
{
    int len = msg.size();
    assert(len > 0);

    // Build and send the control message header
    char hdr[10];
    int j = sizeof(hdr);
    int i = j;
    do {
        hdr[--i] = len & 0x7f;
        len >>= 7;
    } while (len > 0);
    hdr[j-1] |= 0x80;   // High bit indicates last length byte
    hdr[--i] = ControlMarker;
    stream_->write_data(hdr+i, j-i);

    // Send the control message body
    stream_->write_data(msg.data(), msg.size());
}

// Process a control message received as a substream of this shell stream.
shell_stream::packet shell_stream::receive()
{
    while (true) {
        // Fill the receive buffer if it's empty
        if (rx_amount_ == 0) {
            rx_buffer_ = stream_->read_data();
            rx_data_ = rx_buffer_.data();
            rx_amount_ = rx_buffer_.size();
            if (rx_amount_ == 0) {
                // nothing to read at the moment
                return packet();
            }
        }

        // Process the received data
        switch (rstate) {
        case RecvNormal: {
            if (rx_data_[0] == ControlMarker) {
                logger::debug() << "got control marker";
                rstate = RecvLength;
                ctl_len_ = 0;
                rx_data_++, rx_amount_--;
                break;
            }

            // Receive normal character data
            char *p = (char*)memchr(rx_data_, ControlMarker, rx_amount_);
            size_t act = p ? p - rx_data_ : rx_amount_;
            byte_array ret = (act == rx_buffer_.size())
                        ? rx_buffer_
                        : byte_array(rx_data_, act);
            rx_data_ += act, rx_amount_ -= act;
            return packet(packet_type::Data, ret); }

        case RecvLength: {
            if (ctl_len_ >= maxControlMessage) {
                on_error("Control message too large");
                stream_->shutdown(ssu::stream::shutdown_mode::reset);
                return packet();
            }
            char ch = rx_data_[0];
            rx_data_++, rx_amount_--;
            ctl_len_ = (ctl_len_ << 7) | (ch & 0x7f);
            if (ch & 0x80) {
                if (ctl_len_ == 0) {
                    // Just an escaped control marker.
                    return packet(packet_type::Data, {ControlMarker});
                }
                //logger::debug() << "control msg size" << ctl_len_;
                rstate = RecvMessage;
                ctl_buffer_.resize(ctl_len_);
                ctl_got_ = 0;
            }
            break; }

        case RecvMessage: {
            // Receive control message data.
            int act = std::min(rx_amount_, ctl_len_-ctl_got_);
            memcpy(ctl_buffer_.data()+ctl_got_, rx_data_, act);
            rx_data_ += act, rx_amount_ -= act;
            ctl_got_ += act;
            if (ctl_got_ == ctl_len_) {
                // Got a complete control message.
                assert(ctl_buffer_.size() == ctl_len_);
                rstate = RecvNormal;
                packet p(packet_type::Control, ctl_buffer_);
                ctl_buffer_.clear();
                return p;
            }
            break; }
        }
    }
}

bool shell_stream::at_end() const
{
    return stream_->at_end();
}
