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
	empty_frame   ef;
	stream_frame  sf;
	ack_frame	  af;
	padding_frame pf;
	decongestion_frame dcf;
	detach_frame  dtf;
	reset_frame	  rf;
	close_frame   cf;
	settings_frame sef;
	priority_frame prf;	


/*
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
*/
	buf = fusionary::write(buf, ef);
	buf = fusionary::write(buf, sef);
	buf = fusionary::write(buf, af);
	buf = fusionary::write(buf, pf);
	buf = fusionary::write(buf, dcf);
	buf = fusionary::write(buf, dtf);
	buf = fusionary::write(buf, rf);
	buf = fusionary::write(buf, cf);
	buf = fusionary::write(buf, sef);
	buf = fusionary::write(buf, prf);


	BOOST_CHECK(empty_frame::read(buf) == ef);
	BOOST_CHECK(stream_frame::read(buf) == sf);
	BOOST_CHECK(ack_frame::read(buf) == af);
	BOOST_CHECK(padding_frame::read(buf) == pf);
	BOOST_CHECK(decongestion_frame::read(buf) == dcf);
	BOOST_CHECK(detach_frame::read(buf) == dtf);
	BOOST_CHECK(reset_frame::read(buf) == rf);
	BOOST_CHECK(close_frame::read(buf) == cf);
	BOOST_CHECK(settings_frame::read(buf) == sef);
	BOOST_CHECK(priority_frame::read(buf) == prf);

    logger::file_dump(data, "frames serialization test");
}
