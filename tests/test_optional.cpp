//
// Part of Metta OS. Check http://metta.exquance.com for latest version.
//
// Copyright 2007 - 2013, Stanislav Karchebnyy <berkus@exquance.com>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#define BOOST_TEST_MODULE Test_optional_serialization
#include <boost/test/unit_test.hpp>
#include <boost/optional/optional.hpp>
#include "byte_array.h"
#include "byte_array_wrap.h"
#include "flurry.h"
#include "logging.h"

using namespace std;

BOOST_AUTO_TEST_CASE(serialize_and_deserialize)
{
    byte_array data;
    uint32_t i = 0xabbadead;
    boost::optional<uint32_t> maybe_value;

    {
        byte_array_owrap<flurry::oarchive> write(data);

        BOOST_CHECK(maybe_value.is_initialized() == false);
        write.archive() << maybe_value;
        maybe_value = i;
        BOOST_CHECK(maybe_value.is_initialized() == true);
        BOOST_CHECK(*maybe_value == 0xabbadead);
        write.archive() << maybe_value;
    }
    {
        logger::file_dump out(data);
    }
    {
        byte_array_iwrap<flurry::iarchive> read(data);

        // BOOST_CHECK(data.size() == 6);
        read.archive() >> maybe_value;
        BOOST_CHECK(maybe_value.is_initialized() == false);
        read.archive() >> maybe_value;
        BOOST_CHECK(maybe_value.is_initialized() == true);
        BOOST_CHECK(*maybe_value == 0xabbadead);
    }
}
