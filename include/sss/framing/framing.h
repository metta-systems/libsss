#include "packet_frame.h"

/**
 * Given multiple packets to send and a packet buffer, figure out most efficient packing,
 * complying to security policy etc. and write those packets into the buffer.
 * Written packets are cleared from the queue (and inserted into the wait queue for ack if needed).
 * Prepared buffer is forwarded to channel layer for encryption.
 */
class framing
{
    std::deque<shared_ptr<packet_frame_t>> packets;

    void send_packet(shared_ptr<packet_frame_t> f);

    void enframe(asio::mutable_buffer output);
    void deframe(asio::const_buffer input);
};
