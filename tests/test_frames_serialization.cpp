//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#define BOOST_TEST_MODULE Test_frames_serialization
#include "arsenal/byte_array.h"
#include "arsenal/byte_array_wrap.h"
#include "arsenal/flurry.h"
#include "arsenal/logging.h"
#include "sss/framing/frame_format.h"
#include "sss/framing/ack_frame.h"
#include "sss/framing/close_frame.h"
#include "sss/framing/decongestion_frame.h"
#include "sss/framing/detach_frame.h"
#include "sss/framing/empty_frame.h"
#include "sss/framing/packet_frame.h"
#include "sss/framing/padding_frame.h"
#include "sss/framing/priority_frame.h"
#include "sss/framing/reset_frame.h"
#include "sss/framing/settings_frame.h"
#include "sss/framing/stream_frame.h"

#include <boost/test/unit_test.hpp>

#include <fstream>

using namespace std;
using namespace sss::framing;

BOOST_AUTO_TEST_CASE(serialize_frames)
{
    char b[5000];
    empty_frame_t empty, empty2;
    stream_frame_t stream, stream2;
    ack_frame_t ack, ack2;
    padding_frame_t padding, padding2;
    decongestion_frame_t decongestion, decongestion2;
    detach_frame_t detach, detach2;
    reset_frame_t reset, reset2;
    close_frame_t close, close2;
    settings_frame_t settings, settings2;
    priority_frame_t priority, priority2;

    boost::asio::mutable_buffer buf(b, 5000);
    empty.write(buf);
    settings.write(buf);
    ack.write(buf);
    padding.write(buf);
    decongestion.write(buf);
    detach.write(buf);
    reset.write(buf);
    close.write(buf);
    settings.write(buf);
    priority.write(buf);

    boost::asio::const_buffer rbuf(b, 5000);
    empty2.read(rbuf);
    stream2.read(rbuf);
    ack2.read(rbuf);
    padding2.read(rbuf);
    decongestion2.read(rbuf);
    detach2.read(rbuf);
    reset2.read(rbuf);
    close2.read(rbuf);
    settings2.read(rbuf);
    priority2.read(rbuf);

    BOOST_CHECK(empty2 == empty);
    BOOST_CHECK(stream2 == stream);
    BOOST_CHECK(ack2 == ack);
    BOOST_CHECK(padding2 == padding);
    BOOST_CHECK(decongestion2 == decongestion);
    BOOST_CHECK(detach2 == detach);
    BOOST_CHECK(reset2 == reset);
    BOOST_CHECK(close2 == close);
    BOOST_CHECK(settings2 == settings);
    BOOST_CHECK(priority2 == priority);
}
