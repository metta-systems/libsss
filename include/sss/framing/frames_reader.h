//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2015, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include <iostream>
#include <boost/asio.hpp>
#include "sss/framing/frame_format.h"

// Read frames from the packet buffer until we run out.
// Read starts with packet header and continues with different packet and frame types.
class frames_reader
{
    boost::asio::const_buffer buf_;

public:
    frames_reader(std::string const& str)
        : buf_(str.data(), str.size())
    {}

    frames_reader(boost::asio::const_buffer buf)
        : buf_(std::move(buf))
    {}

    void read_packet_header()
    {
        sss::framing::packet_header hdr;
        std::tie(hdr, buf_) = fusionary::read(hdr, buf_);

        std::cout << "Protocol version " << std::showbase << std::hex << hdr.version.value() << std::endl
                  << "Packet sequence  " << std::showbase << std::hex << hdr.packet_sequence.value() << std::endl;
    }

    void read_frame_header()
    {
    }
};

