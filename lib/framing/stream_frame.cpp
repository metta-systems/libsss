#include "arsenal/fusionary.hpp"

using namespace std;
namespace asio = boost::asio;
namespace mpl = boost::mpl;

// Write STREAM frame.
//            0 :                   1 : Frame type (1 - STREAM)
//            1 :                   1 : Flags (niuooodf)
//            2 :                   4 : Stream ID
//            6 :                   4 : Parent Stream ID (optional, when INIT (i) bit is set)
//           10 :                  24 : Stream USID (optional, when USID (u) bit is set)
//     10,14,34 : 0,2,3,4,5,6,7,8 (O) : Offset in stream (depending on OFFSET (ooo) bits)
// (10,14,34)+O :                   2 : Data length (optional, when DATA LENGTH (d) bit is set) D
// (12,16,36)+O :                   D : Data

constexpr uint8_t no_ack_bit = 0b1000'0000; //'
constexpr uint8_t init_bit   = 0b0100'0000; //'
constexpr uint8_t usid_bit   = 0b0010'0000; //'
constexpr uint8_t data_bit   = 0b0000'0010; //'
constexpr uint8_t fini_bit   = 0b0000'0001; //'

bool stream_frame_t::write(asio::mutable_buffer output, stream_frame_header_t hdr, string data,
                         bool no_ack, bool init, bool end, bool usid)
{
    hdr.flags &= 0b0001'1110; //'
    if (no_ack) hdr.flags |= no_ack_bit;
    if (init) {
        hdr.flags |= init_bit;
        if (usid) {
            hdr.flags |= usid_bit;
        }
    }
    if (end) hdr.flags |= fini_bit;
    hdr.data_length = data.size();
    // @todo Determine when could omit data_length field
    // heuristic: if data is going to be clipped by packet size by at least 2 extra bytes.

    uint8_t bits = 0;
    if (hdr.stream_offset == 0) {
        bits = 0;
    } else if (hdr.stream_offset < 0x10000) {
        bits = 0b00100;
    } else if (hdr.stream_offset < 0x1000000) {
        bits = 0b01000;
    } else if (hdr.stream_offset < 0x100000000) {
        bits = 0b01100;
    } else if (hdr.stream_offset < 0x10000000000) {
        bits = 0b10000;
    } else if (hdr.stream_offset < 0x1000000000000) {
        bits = 0b10100;
    } else if (hdr.stream_offset < 0x100000000000000) {
        bits = 0b11000;
    } else {
        bits = 0b11100;
    }
    hdr.flags = (hdr.flags & 0b11100011) | bits;

    write(output, hdr.type);
    write(output, hdr.flags);
    write(output, hdr.stream_id); // @todo BIG endian
    if (hdr.flags & init_bit) {
        write(output, hdr.parent_stream_id);
        if (hdr.flags & usid_bit) {
            write(output, hdr.usid);
        }
    }
    //
    if (hdr.stream_offset == 0) {
    } else if (hdr.stream_offset < 0x10000) {
        uint16_t v = hdr.stream_offset;
        write(output, v);
    } else if (hdr.stream_offset < 0x1000000) {
        throw "3 bytes write unimplemented";
    } else if (hdr.stream_offset < 0x100000000) {
        uint32_t v = hdr.stream_offset;
        write(output, v);
    } else if (hdr.stream_offset < 0x10000000000) {
        throw "5 bytes write unimplemented";
    } else if (hdr.stream_offset < 0x1000000000000) {
        throw "6 bytes write unimplemented";
    } else if (hdr.stream_offset < 0x100000000000000) {
        throw "7 bytes write unimplemented";
    } else {
        write(output, hdr.stream_offset);
    }
    //
    if (hdr.flags & data_bit) {
        write(output, hdr.data_length);
    }
    write_buffer(output, data);
}

bool stream_frame_t::read(asio::const_buffer input)
{
}
