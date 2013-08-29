//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/optional/optional.hpp>
#include "custom_optional.h" // optional value serialization instead of boost version
#include "byte_array.h"
#define BOOST_TEST_MODULE Test_optional_serialization
#include <boost/test/unit_test.hpp>

using namespace std;

BOOST_AUTO_TEST_CASE(serialize_and_deserialize)
{
    byte_array data;
    uint32_t i = 0xabbadead;
    boost::optional<uint32_t> maybe_value;

    {
        byte_array_owrap<boost::archive::binary_oarchive> w(data);

        BOOST_CHECK(maybe_value.is_initialized() == false);
        w.archive() << maybe_value;
        maybe_value = i;
        BOOST_CHECK(maybe_value.is_initialized() == true);
        BOOST_CHECK(*maybe_value == 0xabbadead);
        w.archive() << maybe_value;
    }
    {
        byte_array_iwrap<boost::archive::binary_iarchive> r(data);

        BOOST_CHECK(data.size() == 6);
        r.archive() >> maybe_value;
        BOOST_CHECK(maybe_value.is_initialized() == false);
        r.archive() >> maybe_value;
        BOOST_CHECK(maybe_value.is_initialized() == true);
        BOOST_CHECK(*maybe_value == 0xabbadead);
    }
}
