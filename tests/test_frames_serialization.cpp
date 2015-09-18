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
	char b[5000] = {
        '\0',
        '\1',
            '\31', 
            '\0', '\0', '\0', '\1', 
    };
	empty_frame_t ef, ef2;
	stream_frame_t sf, sf2;
	ack_frame_t af, af2;
	padding_frame_t pf, pf2;
	decongestion_frame_t dcf, dcf2;
	detach_frame_t dtf, dtf2;
	reset_frame_t rf, rf2;
	close_frame_t cf, cf2;
	settings_frame_t sef, sef2;
	priority_frame_t prf, prf2;

	boost::asio::const_buffer rbuf(b, 5000);
    ef2.read(rbuf);
    sf2.read(rbuf);
    af2.read(rbuf);
    pf2.read(rbuf);
    dcf2.read(rbuf);
    dtf2.read(rbuf);
    rf2.read(rbuf);
    cf2.read(rbuf);
    sef2.read(rbuf);
    prf2.read(rbuf);


	boost::asio::mutable_buffer buf(b, 5000);
	ef.write(buf);
	sf.write(buf);
	af.write(buf);
	pf.write(buf);
	dcf.write(buf);
	dtf.write(buf);
	rf.write(buf);
	cf.write(buf);
	sef.write(buf);
	prf.write(buf);

	BOOST_CHECK(ef2 == ef);
	BOOST_CHECK(sf2 == sf);
	BOOST_CHECK(af2 == af);
	BOOST_CHECK(pf2 == pf);
	BOOST_CHECK(dcf2 == dcf);
	BOOST_CHECK(dtf2 == dtf);
	BOOST_CHECK(rf2 == rf);
	BOOST_CHECK(cf2 == cf);
	BOOST_CHECK(sef2 == sef);
	BOOST_CHECK(prf2 == prf);
}
