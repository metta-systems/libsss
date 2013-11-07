#pragma once

#include <boost/signals2/signal.hpp>
#include "shell_protocol.h"
#include "stream.h"

// Common base class for managing both client- and server-side shell streams.
// Handles encoding and decoding control messages embedded within the stream.
class shell_stream : public shell_protocol
{
public:
    enum class packet_type { Null, Data, Control };
    struct packet {
        packet_type type;
        byte_array data;

        inline packet(packet_type type = packet_type::Null, byte_array data = byte_array())
            : type(type), data(data)
        {}
        inline bool is_null() { return type == packet_type::Null; }
    };

private:
    static constexpr int maxControlMessage = 1<<24;

    ssu::stream *stream_{nullptr}; // Handle onto a control stream (we do not own it)

    // Receive state:
    //  0: normal character transmission
    //  1: received SOH, expecting control message length byte(s)
    //  2: received control message length, expecting message byte(s)
    enum {
        RecvNormal, // normal character transmission
        RecvLength, // got SOH, expecting control message length
        RecvMessage,    // Got SOH and length, expecting message data
    } rstate;

    byte_array rx_buffer_;    // Raw stream data receive buffer, rbuf
    char *rx_data_;
    int rx_amount_{0};//ramt

    byte_array ctl_buffer_;    // Control message buffer
    int ctl_len_, ctl_got_;

public:
    shell_stream(ssu::stream *strm = nullptr);

    inline ssu::stream *stream() { return stream_; }
    void set_stream(ssu::stream *stream);

    packet receive();
    bool at_end() const;

    void send_data(const char *data, int size);
    inline void send_data(const byte_array &buf) {
        send_data(buf.data(), buf.size());
    }

    void send_control(byte_array const& msg);
    void send_eof();

    // signals:
    typedef boost::signals2::signal<void (void)> ready_signal;
    ready_signal on_ready_read;

    typedef boost::signals2::signal<void (ssize_t)> bytes_written_signal;
    bytes_written_signal on_bytes_written;

    typedef boost::signals2::signal<void (std::string const&)> error_signal;
    /**
     * Emitted when a protocol error is detected.
     */
    error_signal on_error;
};
