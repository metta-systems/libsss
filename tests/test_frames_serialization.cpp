//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#define BOOST_TEST_MODULE Test_frames_serialization
#include <boost/test/unit_test.hpp>
#include <fstream>
#include "arsenal/byte_array.h"
#include "arsenal/byte_array_wrap.h"
#include "arsenal/flurry.h"
#include "arsenal/logging.h"
#include "sss/framing/frame_format.h"

using namespace std;

BOOST_AUTO_TEST_CASE(serialize_frames)
{
    std::vector<char> b(5000);
    boost::asio::buffer buf(b);
    empty_frame empty;
    stream_frame stream;
    ack_frame ack;
    padding_frame padding;
    decongestion_frame decongestion;
    detach_frame detach;
    reset_frame reset;
    close_frame close;
    settings_frame settings;
    priority_frame priority;

    /*
        empty.write(buf);
        stream.write(buf);
        ack.write(buf);
        padding.write(buf);
        decongestion.write(buf);
        detach.write(buf);
        reset.write(buf);
        close.write(buf);
        settings.write(buf);
        priority.write(buf);
    */
    buf = fusionary::write(buf, empty);
    buf = fusionary::write(buf, settings);
    buf = fusionary::write(buf, ack);
    buf = fusionary::write(buf, padding);
    buf = fusionary::write(buf, decongestion);
    buf = fusionary::write(buf, detach);
    buf = fusionary::write(buf, reset);
    buf = fusionary::write(buf, close);
    buf = fusionary::write(buf, settings);
    buf = fusionary::write(buf, priority);

    BOOST_CHECK(empty_frame::read(buf) == empty);
    BOOST_CHECK(stream_frame::read(buf) == stream);
    BOOST_CHECK(ack_frame::read(buf) == af);
    BOOST_CHECK(padding_frame::read(buf) == padding);
    BOOST_CHECK(decongestion_frame::read(buf) == decongestion);
    BOOST_CHECK(detach_frame::read(buf) == detach);
    BOOST_CHECK(reset_frame::read(buf) == reset);
    BOOST_CHECK(close_frame::read(buf) == close);
    BOOST_CHECK(settings_frame::read(buf) == settings);
    BOOST_CHECK(priority_frame::read(buf) == priority);

    logger::file_dump(data, "frames serialization test");
}
