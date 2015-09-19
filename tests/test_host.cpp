//
// Part of Metta OS. Check http://atta-metta.net for latest version.
//
// Copyright 2007 - 2014, Stanislav Karchebnyy <berkus@atta-metta.net>
//
// Distributed under the Boost Software License, Version 1.0.
// (See file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt)
//
#define BOOST_TEST_MODULE Test_sss_host
#include <boost/test/unit_test.hpp>

#include "sss/host.h"

using namespace sss;

BOOST_AUTO_TEST_CASE(created_host)
{
    std::shared_ptr<host> my_host(host::create());
}
